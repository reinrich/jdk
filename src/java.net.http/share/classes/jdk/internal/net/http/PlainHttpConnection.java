/*
 * Copyright (c) 2015, 2025, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package jdk.internal.net.http;

import java.io.IOException;
import java.net.ConnectException;
import java.net.InetSocketAddress;
import java.net.StandardSocketOptions;
import java.nio.channels.SelectableChannel;
import java.nio.channels.SelectionKey;
import java.nio.channels.SocketChannel;
import java.time.Duration;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.locks.ReentrantLock;
import java.util.function.Function;

import jdk.internal.net.http.common.FlowTube;
import jdk.internal.net.http.common.Log;
import jdk.internal.net.http.common.MinimalFuture;
import jdk.internal.net.http.common.TimeSource;
import jdk.internal.net.http.common.Utils;

/**
 * Plain raw TCP connection direct to destination.
 * The connection operates in asynchronous non-blocking mode.
 * All reads and writes are done non-blocking.
 */
class PlainHttpConnection extends HttpConnection {

    protected final SocketChannel chan;
    private final SocketTube tube; // need SocketTube to call signalClosed().
    private final PlainHttpPublisher writePublisher = new PlainHttpPublisher();
    private volatile boolean connected;
    private volatile boolean closed;
    private volatile ConnectTimerEvent connectTimerEvent;  // may be null
    private volatile int unsuccessfulAttempts;
    private final ReentrantLock stateLock = new ReentrantLock();
    private final AtomicReference<Throwable> errorRef = new AtomicReference<>();

    // Indicates whether a connection attempt has succeeded or should be retried.
    // If the attempt failed, and shouldn't be retried, there will be an exception
    // instead.
    private enum ConnectState { SUCCESS, RETRY }


    /**
     * Returns a ConnectTimerEvent iff there is a connect timeout duration,
     * otherwise null.
     */
    private ConnectTimerEvent newConnectTimer(Exchange<?> exchange,
                                              CompletableFuture<?> cf) {
        Duration duration = exchange.remainingConnectTimeout().orElse(null);
        if (duration != null) {
            ConnectTimerEvent cte = new ConnectTimerEvent(duration, exchange, cf);
            return cte;
        }
        return null;
    }

    final class ConnectTimerEvent extends TimeoutEvent {
        private final CompletableFuture<?> cf;
        private final Exchange<?> exchange;

        ConnectTimerEvent(Duration duration,
                          Exchange<?> exchange,
                          CompletableFuture<?> cf) {
            super(duration);
            this.exchange = exchange;
            this.cf = cf;
        }

        @Override
        public void handle() {
            if (debug.on()) {
                debug.log("HTTP connect timed out");
            }
            ConnectException ce = new ConnectException("HTTP connect timed out");
            exchange.multi.cancel(ce);
            client().theExecutor().execute(() -> cf.completeExceptionally(ce));
        }

        @Override
        public String toString() {
            return "ConnectTimerEvent, " + super.toString();
        }
    }

    Throwable getError(Throwable cause) {
        if (errorRef.compareAndSet(null, cause)) return cause;
        return errorRef.get();
    }

    final class ConnectEvent extends AsyncEvent {
        private final CompletableFuture<ConnectState> cf;
        private final Exchange<?> exchange;

        ConnectEvent(CompletableFuture<ConnectState> cf, Exchange<?> exchange) {
            this.cf = cf;
            this.exchange = exchange;
        }

        @Override
        public SelectableChannel channel() {
            return chan;
        }

        @Override
        public int interestOps() {
            return SelectionKey.OP_CONNECT;
        }

        @Override
        public void handle() {
            try {
                assert !connected : "Already connected";
                assert !chan.isBlocking() : "Unexpected blocking channel";
                if (debug.on())
                    debug.log("ConnectEvent: finishing connect");
                boolean finished = chan.finishConnect();
                if (debug.on())
                    debug.log("ConnectEvent: connect finished: %s, cancelled: %s, Local addr: %s",
                              finished, exchange.multi.requestCancelled(), chan.getLocalAddress());
                assert finished || exchange.multi.requestCancelled() : "Expected channel to be connected";
                if (connectionOpened()) {
                    // complete async since the event runs on the SelectorManager thread
                    cf.completeAsync(() -> ConnectState.SUCCESS, client().theExecutor());
                } else throw new ConnectException("Connection closed");
            } catch (Throwable e) {
                if (canRetryConnect(e)) {
                    unsuccessfulAttempts++;
                    // complete async since the event runs on the SelectorManager thread
                    cf.completeAsync(() -> ConnectState.RETRY, client().theExecutor());
                    return;
                }
                Throwable t = getError(Utils.toConnectException(e));
                // complete async since the event runs on the SelectorManager thread
                client().theExecutor().execute( () -> cf.completeExceptionally(t));
                close(t);
            }
        }

        @Override
        public void abort(IOException ioe) {
            Throwable cause = getError(ioe);
            // complete async since the event runs on the SelectorManager thread
            client().theExecutor().execute( () -> cf.completeExceptionally(cause));
            close(cause);
        }
    }

    @Override
    public CompletableFuture<Void> connectAsync(Exchange<?> exchange) {
        CompletableFuture<ConnectState> cf = new MinimalFuture<>();
        try {
            assert !connected : "Already connected";
            assert !chan.isBlocking() : "Unexpected blocking channel";
            boolean finished;

            if (connectTimerEvent == null) {
                connectTimerEvent = newConnectTimer(exchange, cf);
                if (connectTimerEvent != null) {
                    if (debug.on())
                        debug.log("registering connect timer: " + connectTimerEvent);
                    client().registerTimer(connectTimerEvent);
                }
            }

            var localAddr = client().localAddress();
            if (localAddr != null) {
                if (debug.on()) {
                    debug.log("binding to configured local address " + localAddr);
                }
                var sockAddr = new InetSocketAddress(localAddr, 0);
                try {
                    chan.bind(sockAddr);
                    if (debug.on()) {
                        debug.log("bind completed " + localAddr);
                    }
                } catch (IOException cause) {
                    if (debug.on()) {
                        debug.log("bind to " + localAddr + " failed: " + cause.getMessage());
                    }
                    throw cause;
                }
            }

            finished = chan.connect(Utils.resolveAddress(address));
            if (finished) {
                if (debug.on()) debug.log("connect finished without blocking");
                if (connectionOpened()) {
                    cf.complete(ConnectState.SUCCESS);
                } else throw getError(new ConnectException("connection closed"));
            } else {
                if (debug.on()) debug.log("registering connect event");
                client().registerEvent(new ConnectEvent(cf, exchange));
            }
            cf = exchange.checkCancelled(cf, this);
        } catch (Throwable throwable) {
            var cause = getError(Utils.toConnectException(throwable));
            cf.completeExceptionally(cause);
            try {
                if (Log.channel()) {
                    Log.logChannel("Closing connection: connect failed due to: " + throwable);
                }
                close(cause);
            } catch (Exception x) {
                if (debug.on())
                    debug.log("Failed to close channel after unsuccessful connect");
            }
        }
        return cf.handle((r,t) -> checkRetryConnect(r, t, exchange))
                .thenCompose(Function.identity());
    }

    boolean connectionOpened() {
        boolean closed = this.closed;
        if (closed) return false;
        stateLock.lock();
        try {
            closed = this.closed;
            if (!closed) {
                client().connectionOpened(this);
            }
            // connectionOpened might call close() if the client
            // is shutting down.
            closed = this.closed;
        } finally {
            stateLock.unlock();
        }
        return !closed;
    }

    /**
     * On some platforms, a ConnectEvent may be raised and a ConnectionException
     * may occur with the message "Connection timed out: no further information"
     * before our actual connection timeout has expired. In this case, this
     * method will be called with a {@code connect} state of {@code ConnectState.RETRY)}
     * and we will retry once again.
     * @param connect indicates whether the connection was successful or should be retried
     * @param failed the failure if the connection failed
     * @param exchange the exchange
     * @return a completable future that will take care of retrying the connection if needed.
     */
    private CompletableFuture<Void> checkRetryConnect(ConnectState connect, Throwable failed, Exchange<?> exchange) {
        // first check if the connection failed
        if (failed != null) return MinimalFuture.failedFuture(failed);
        // then check if the connection should be retried
        if (connect == ConnectState.RETRY) {
            int attempts = unsuccessfulAttempts;
            assert attempts <= 1;
            if (debug.on())
                debug.log("Retrying connect after %d attempts", attempts);
            return connectAsync(exchange);
        }
        // Otherwise, the connection was successful;
        assert connect == ConnectState.SUCCESS;
        return MinimalFuture.completedFuture(null);
    }

    private boolean canRetryConnect(Throwable e) {
        if (!MultiExchange.RETRY_CONNECT) return false;
        if (!(e instanceof ConnectException)) return false;
        if (unsuccessfulAttempts > 0) return false;
        ConnectTimerEvent timer = connectTimerEvent;
        if (timer == null) return true;
        return timer.deadline().isAfter(TimeSource.now());
    }

    @Override
    public CompletableFuture<Void> finishConnect() {
        assert connected == false;
        if (debug.on()) debug.log("finishConnect, setting connected=true");
        connected = true;
        if (connectTimerEvent != null)
            client().cancelTimer(connectTimerEvent);
        return MinimalFuture.completedFuture(null);
    }

    @Override
    SocketChannel channel() {
        return chan;
    }

    @Override
    final FlowTube getConnectionFlow() {
        return tube;
    }

    PlainHttpConnection(Origin originServer, InetSocketAddress addr, HttpClientImpl client,
                        String label) {
        super(originServer, addr, client, label);
        try {
            this.chan = SocketChannel.open();
            chan.configureBlocking(false);
            if (debug.on()) {
                int bufsize = getSoReceiveBufferSize();
                debug.log("Initial receive buffer size is: %d", bufsize);
                bufsize = getSoSendBufferSize();
                debug.log("Initial send buffer size is: %d", bufsize);
            }
            if (trySetReceiveBufferSize(client.getReceiveBufferSize())) {
                if (debug.on()) {
                    int bufsize = getSoReceiveBufferSize();
                    debug.log("Receive buffer size configured: %d", bufsize);
                }
            }
            if (trySetSendBufferSize(client.getSendBufferSize())) {
                if (debug.on()) {
                    int bufsize = getSoSendBufferSize();
                    debug.log("Send buffer size configured: %d", bufsize);
                }
            }
            chan.setOption(StandardSocketOptions.TCP_NODELAY, true);
            // wrap the channel in a Tube for async reading and writing
            tube = new SocketTube(client(), chan, Utils::getBuffer, label);
        } catch (IOException e) {
            throw new InternalError(e);
        }
    }

    private int getSoReceiveBufferSize() {
        try {
            return chan.getOption(StandardSocketOptions.SO_RCVBUF);
        } catch (IOException x) {
            if (debug.on())
                debug.log("Failed to get initial receive buffer size on %s", chan);
        }
        return 0;
    }

    private int getSoSendBufferSize() {
        try {
            return chan.getOption(StandardSocketOptions.SO_SNDBUF);
        } catch (IOException x) {
            if (debug.on())
                debug.log("Failed to get initial receive buffer size on %s", chan);
        }
        return 0;
    }

    private boolean trySetReceiveBufferSize(int bufsize) {
        try {
            if (bufsize > 0) {
                chan.setOption(StandardSocketOptions.SO_RCVBUF, bufsize);
                return true;
            }
        } catch (IOException x) {
            if (debug.on())
                debug.log("Failed to set receive buffer size to %d on %s",
                          bufsize, chan);
        }
        return false;
    }

    private boolean trySetSendBufferSize(int bufsize) {
        try {
            if (bufsize > 0) {
                chan.setOption(StandardSocketOptions.SO_SNDBUF, bufsize);
                return true;
            }
        } catch (IOException x) {
            if (debug.on())
                debug.log("Failed to set send buffer size to %d on %s",
                        bufsize, chan);
        }
        return false;
    }

    @Override
    HttpPublisher publisher() { return writePublisher; }


    @Override
    public String toString() {
        return "PlainHttpConnection: " + super.toString();
    }

    @Override
    public void close() {
        close(null);
    }

    @Override
    void close(Throwable cause) {
        var closed = this.closed;
        if (closed) return;
        stateLock.lock();
        try {
            if (closed = this.closed) return;
            closed = this.closed = true;
            Throwable reason = getError(cause);
            Log.logTrace("Closing: " + toString());
            if (debug.on()) {
                String interestOps = client().debugInterestOps(chan);
                if (reason == null) {
                    debug.log("Closing channel: " + interestOps);
                } else {
                    debug.log("Closing channel: %s due to %s", interestOps, reason);
                }
            }
            var connectTimerEvent = this.connectTimerEvent;
            if (connectTimerEvent != null)
                client().cancelTimer(connectTimerEvent);
            if (Log.channel()) {
                Log.logChannel("Closing channel: " + chan);
            }
            try {
                tube.signalClosed(errorRef.get());
                chan.close();
            } finally {
                client().connectionClosed(this);
            }
        } catch (IOException e) {
            debug.log("Closing resulted in " + e);
            Log.logTrace("Closing resulted in " + e);
        } finally {
            stateLock.unlock();
        }
    }


    @Override
    ConnectionPool.CacheKey cacheKey() {
        return ConnectionPool.cacheKey(false, address, null);
    }

    @Override
    boolean connected() {
        return connected;
    }


    @Override
    boolean isSecure() {
        return false;
    }

    @Override
    boolean isProxied() {
        return false;
    }

    @Override
    InetSocketAddress proxy() {
        return null;
    }
}
