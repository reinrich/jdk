/*
 * Copyright (c) 1997, 2025, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
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
 *
 */

#ifndef SHARE_UTILITIES_BITMAP_HPP
#define SHARE_UTILITIES_BITMAP_HPP

#include "nmt/memTag.hpp"
#include "runtime/atomic.hpp"
#include "utilities/globalDefinitions.hpp"

// Forward decl;
class BitMapClosure;

// Operations for bitmaps represented as arrays of unsigned integers.
// Bits are numbered from 0 to size-1.

// The "abstract" base BitMap class.
//
// The constructor and destructor are protected to prevent
// creation of BitMap instances outside of the BitMap class.
//
// The BitMap class doesn't use virtual calls on purpose,
// this ensures that we don't get a vtable unnecessarily.
//
// The allocation of the backing storage for the BitMap are handled by
// the subclasses. BitMap doesn't allocate or delete backing storage.
class BitMap {
  friend class BitMap2D;

 public:
  typedef size_t idx_t;         // Type used for bit and word indices.
  typedef uintptr_t bm_word_t;  // Element type of array that represents the
                                // bitmap, with BitsPerWord bits per element.
  // If this were to fail, there are lots of places that would need repair.
  STATIC_ASSERT((sizeof(bm_word_t) * BitsPerByte) == BitsPerWord);

  // Hints for range sizes.
  typedef enum {
    unknown_range, small_range, large_range
  } RangeSizeHint;

 private:
  bm_word_t* _map;     // First word in bitmap
  idx_t      _size;    // Size of bitmap (in bits)

 protected:
  // The maximum allowable size of a bitmap, in words or bits.
  // Limit max_size_in_bits so aligning up to a word boundary never overflows.
  constexpr static idx_t max_size_in_words() { return raw_to_words_align_down(~idx_t(0)); }
  constexpr static idx_t max_size_in_bits() { return max_size_in_words() * BitsPerWord; }

  // Assumes relevant validity checking for bit has already been done.
  constexpr static idx_t raw_to_words_align_up(idx_t bit) {
    return raw_to_words_align_down(bit + (BitsPerWord - 1));
  }

  // Assumes relevant validity checking for bit has already been done.
  constexpr static idx_t raw_to_words_align_down(idx_t bit) {
    return bit >> LogBitsPerWord;
  }

  // Word-aligns bit and converts it to a word offset.
  // precondition: bit <= size()
  idx_t to_words_align_up(idx_t bit) const {
    verify_limit(bit);
    return raw_to_words_align_up(bit);
  }

  // Word-aligns bit and converts it to a word offset.
  // precondition: bit <= size()
  inline idx_t to_words_align_down(idx_t bit) const {
    verify_limit(bit);
    return raw_to_words_align_down(bit);
  }

  // Helper for find_first_{set,clear}_bit variants.
  // - flip designates whether searching for 1s or 0s.  Must be one of
  //   find_{zeros,ones}_flip.
  // - aligned_right is true if end is a priori on a bm_word_t boundary.
  // - returns end if not found.
  template<bm_word_t flip, bool aligned_right>
  inline idx_t find_first_bit_impl(idx_t beg, idx_t end) const;

  // Helper for find_last_{set,clear}_bit variants.
  // - flip designates whether searching for 1s or 0s.  Must be one of
  //   find_{zeros,ones}_flip.
  // - aligned_left is true if beg is a priori on a bm_word_t boundary.
  // - returns end if not found.
  template<bm_word_t flip, bool aligned_left>
  inline idx_t find_last_bit_impl(idx_t beg, idx_t end) const;

  // Values for find_{first,last}_bit_impl flip parameter.
  static const bm_word_t find_ones_flip = 0;
  static const bm_word_t find_zeros_flip = ~(bm_word_t)0;

  template<typename ReturnType> struct IterateInvoker;

  struct IteratorImpl;

  // Threshold for performing small range operation, even when large range
  // operation was requested. Measured in words.
  static const size_t small_range_words = 32;

  static bool is_small_range_of_words(idx_t beg_full_word, idx_t end_full_word);

  // Return the position of bit within the word that contains it (e.g., if
  // bitmap words are 32 bits, return a number 0 <= n <= 31).
  static idx_t bit_in_word(idx_t bit) { return bit & (BitsPerWord - 1); }

  // Return a mask that will select the specified bit, when applied to the word
  // containing the bit.
  static bm_word_t bit_mask(idx_t bit) { return (bm_word_t)1 << bit_in_word(bit); }

  // Return the bit number of the first bit in the specified word.
  static idx_t bit_index(idx_t word)  { return word << LogBitsPerWord; }

  // Return the array of bitmap words, or a specific word from it.
  bm_word_t* map()                 { return _map; }
  const bm_word_t* map() const     { return _map; }

  // Return a pointer to the word containing the specified bit.
  bm_word_t* word_addr(idx_t bit) {
    return map() + to_words_align_down(bit);
  }
  const bm_word_t* word_addr(idx_t bit) const {
    return map() + to_words_align_down(bit);
  }

  // Get a word and flip its bits according to flip.
  bm_word_t flipped_word(idx_t word, bm_word_t flip) const {
    return _map[word] ^ flip;
  }

  // Set a word to a specified value or to all ones; clear a word.
  void set_word  (idx_t word, bm_word_t val) { _map[word] = val; }
  void set_word  (idx_t word)            { set_word(word, ~(bm_word_t)0); }
  void clear_word(idx_t word)            { _map[word] = 0; }

  static inline bm_word_t load_word_ordered(const volatile bm_word_t* const addr, atomic_memory_order memory_order);

  // Utilities for ranges of bits.  Ranges are half-open [beg, end).

  // Ranges within a single word.
  bm_word_t inverted_bit_mask_for_range(idx_t beg, idx_t end) const;
  void  set_range_within_word      (idx_t beg, idx_t end);
  void  clear_range_within_word    (idx_t beg, idx_t end);
  void  par_put_range_within_word  (idx_t beg, idx_t end, bool value);

  // Ranges spanning entire words.
  void      set_range_of_words         (idx_t beg, idx_t end);
  void      clear_range_of_words       (idx_t beg, idx_t end);
  void      set_large_range_of_words   (idx_t beg, idx_t end);
  void      clear_large_range_of_words (idx_t beg, idx_t end);

  static void clear_range_of_words(bm_word_t* map, idx_t beg, idx_t end);

  idx_t count_one_bits_within_word(idx_t beg, idx_t end) const;
  idx_t count_one_bits_in_range_of_words(idx_t beg_full_word, idx_t end_full_word) const;

  // Set the map and size.
  void update(bm_word_t* map, idx_t size) {
    _map = map;
    _size = size;
  }

  // Protected constructor and destructor.
  BitMap(bm_word_t* map, idx_t size_in_bits) : _map(map), _size(size_in_bits) {
    verify_size(size_in_bits);
  }
  ~BitMap() {}

 public:
  // Pretouch the entire range of memory this BitMap covers.
  void pretouch();

  // Accessing
  constexpr static idx_t calc_size_in_words(size_t size_in_bits) {
    verify_size(size_in_bits);
    return raw_to_words_align_up(size_in_bits);
  }

  idx_t size() const          { return _size; }
  idx_t size_in_words() const { return calc_size_in_words(size()); }
  idx_t size_in_bytes() const { return size_in_words() * BytesPerWord; }

  bool at(idx_t index) const {
    verify_index(index);
    return (*word_addr(index) & bit_mask(index)) != 0;
  }

  // memory_order must be memory_order_relaxed or memory_order_acquire.
  bool par_at(idx_t index, atomic_memory_order memory_order = memory_order_acquire) const;

  // Set or clear the specified bit.
  inline void set_bit(idx_t bit);
  inline void clear_bit(idx_t bit);

  // Attempts to change a bit to a desired value. The operation returns true if
  // this thread changed the value of the bit. It was changed with a RMW operation
  // using the specified memory_order. The operation returns false if the change
  // could not be set due to the bit already being observed in the desired state.
  // The atomic access that observed the bit in the desired state has acquire
  // semantics, unless memory_order is memory_order_relaxed or memory_order_release.
  inline bool par_set_bit(idx_t bit, atomic_memory_order memory_order = memory_order_conservative);
  inline bool par_clear_bit(idx_t bit, atomic_memory_order memory_order = memory_order_conservative);

  // Put the given value at the given index. The parallel version
  // will CAS the value into the bitmap and is quite a bit slower.
  // The parallel version also returns a value indicating if the
  // calling thread was the one that changed the value of the bit.
  void at_put(idx_t bit, bool value);
  bool par_at_put(idx_t bit, bool value);

  // Update a range of bits.  Ranges are half-open [beg, end).
  void set_range   (idx_t beg, idx_t end);
  void clear_range (idx_t beg, idx_t end);
  void set_large_range   (idx_t beg, idx_t end);
  void clear_large_range (idx_t beg, idx_t end);
  void at_put_range(idx_t beg, idx_t end, bool value);
  void par_at_put_range(idx_t beg, idx_t end, bool value);
  void at_put_large_range(idx_t beg, idx_t end, bool value);
  void par_at_put_large_range(idx_t beg, idx_t end, bool value);

  // Update a range of bits, using a hint about the size.  Currently only
  // inlines the predominant case of a 1-bit range.  Works best when hint is a
  // compile-time constant.
  void set_range(idx_t beg, idx_t end, RangeSizeHint hint);
  void clear_range(idx_t beg, idx_t end, RangeSizeHint hint);
  void par_set_range(idx_t beg, idx_t end, RangeSizeHint hint);
  void par_clear_range  (idx_t beg, idx_t end, RangeSizeHint hint);

  // Clearing
  void clear_large();
  inline void clear();

  // Verification.

  // Verify size_in_bits does not exceed max_size_in_bits().
  constexpr static void verify_size(idx_t size_in_bits) {
#ifdef ASSERT
    assert(size_in_bits <= max_size_in_bits(),
           "out of bounds: %zu", size_in_bits);
#endif
  }

  // Verify bit is less than size().
  void verify_index(idx_t bit) const NOT_DEBUG_RETURN;
  // Verify bit is not greater than size().
  void verify_limit(idx_t bit) const NOT_DEBUG_RETURN;
  // Verify [beg,end) is a valid range, e.g. beg <= end <= size().
  void verify_range(idx_t beg, idx_t end) const NOT_DEBUG_RETURN;

  // Applies an operation to the index of each set bit in [beg, end), in
  // increasing (decreasing for reverse iteration) order.
  //
  // If i is an index of the bitmap, the operation is either
  // - function(i)
  // - cl->do_bit(i)
  // The result of an operation must be either void or convertible to bool.
  //
  // If an operation returns false then the iteration stops at that index.
  // The result of the iteration is true unless the iteration was stopped by
  // an operation returning false.
  //
  // If an operation modifies the bitmap, modifications to bits at indices
  // greater than (less than for reverse iteration) the current index will
  // affect which further indices the operation will be applied to.
  //
  // See also the Iterator and ReverseIterator classes.
  //
  // precondition: beg and end form a valid range for the bitmap.
  template<typename Function>
  bool iterate(Function function, idx_t beg, idx_t end) const;

  template<typename BitMapClosureType>
  bool iterate(BitMapClosureType* cl, idx_t beg, idx_t end) const;

  template<typename Function>
  bool iterate(Function function) const {
    return iterate(function, 0, size());
  }

  template<typename BitMapClosureType>
  bool iterate(BitMapClosureType* cl) const {
    return iterate(cl, 0, size());
  }

  template<typename Function>
  bool reverse_iterate(Function function, idx_t beg, idx_t end) const;

  template<typename BitMapClosureType>
  bool reverse_iterate(BitMapClosureType* cl, idx_t beg, idx_t end) const;

  template<typename Function>
  bool reverse_iterate(Function function) const {
    return reverse_iterate(function, 0, size());
  }

  template<typename BitMapClosureType>
  bool reverse_iterate(BitMapClosureType* cl) const {
    return reverse_iterate(cl, 0, size());
  }

  class Iterator;
  class ReverseIterator;
  class RBFIterator;
  class ReverseRBFIterator;

  // Return the index of the first set (or clear) bit in the range [beg, end),
  // or end if none found.
  // precondition: beg and end form a valid range for the bitmap.
  idx_t find_first_set_bit(idx_t beg, idx_t end) const;
  idx_t find_first_clear_bit(idx_t beg, idx_t end) const;

  idx_t find_first_set_bit(idx_t beg) const {
    return find_first_set_bit(beg, size());
  }
  idx_t find_first_clear_bit(idx_t beg) const {
    return find_first_clear_bit(beg, size());
  }

  // Like "find_first_set_bit", except requires that "end" is
  // aligned to bitsizeof(bm_word_t).
  idx_t find_first_set_bit_aligned_right(idx_t beg, idx_t end) const;

  // Return the index of the last set (or clear) bit in the range [beg, end),
  // or end if none found.
  // precondition: beg and end form a valid range for the bitmap.
  idx_t find_last_set_bit(idx_t beg, idx_t end) const;
  idx_t find_last_clear_bit(idx_t beg, idx_t end) const;

  idx_t find_last_set_bit(idx_t beg) const {
    return find_last_set_bit(beg, size());
  }
  idx_t find_last_clear_bit(idx_t beg) const {
    return find_last_clear_bit(beg, size());
  }

  // Like "find_last_set_bit", except requires that "beg" is
  // aligned to bitsizeof(bm_word_t).
  idx_t find_last_set_bit_aligned_left(idx_t beg, idx_t end) const;

  // Returns the number of bits set in the bitmap.
  idx_t count_one_bits() const;

  // Returns the number of bits set within  [beg, end).
  idx_t count_one_bits(idx_t beg, idx_t end) const;

  // Set operations.
  void set_union(const BitMap& bits);
  void set_difference(const BitMap& bits);
  void set_intersection(const BitMap& bits);
  // Returns true iff "this" is a superset of "bits".
  bool contains(const BitMap& bits) const;
  // Returns true iff "this and "bits" have a non-empty intersection.
  bool intersects(const BitMap& bits) const;

  // Returns result of whether this map changed
  // during the operation
  bool set_union_with_result(const BitMap& bits);
  bool set_difference_with_result(const BitMap& bits);
  bool set_intersection_with_result(const BitMap& bits);

  void set_from(const BitMap& bits);

  bool is_same(const BitMap& bits) const;

  // Test if all bits are set or cleared
  bool is_full() const;
  bool is_empty() const;

  void write_to(bm_word_t* buffer, size_t buffer_size_in_bytes) const;

  // Printing
  void print_range_on(outputStream* st, const char* prefix) const;
  void print_on(outputStream* st) const;
};

// Implementation support for bitmap iteration.  While it could be used to
// support bi-directional iteration, it is only intended to be used for
// uni-directional iteration.  The directionality is determined by the using
// class.
struct BitMap::IteratorImpl {
  const BitMap* _map;
  idx_t _cur_beg;
  idx_t _cur_end;

  void assert_not_empty() const NOT_DEBUG_RETURN;

  // Constructs an empty iterator.
  IteratorImpl();

  // Constructs an iterator for map, over the range [beg, end).
  // May be constructed for one of forward or reverse iteration.
  // precondition: beg and end form a valid range for map.
  // precondition: either beg == end or
  // (1) if for forward iteration, then beg must designate a set bit,
  // (2) if for reverse iteration, then end-1 must designate a set bit.
  IteratorImpl(const BitMap* map, idx_t beg, idx_t end);

  // Returns true if the remaining iteration range is empty.
  bool is_empty() const;

  // Returns the index of the first set bit in the remaining iteration range.
  // precondition: !is_empty()
  // precondition: constructed for forward iteration.
  idx_t first() const;

  // Returns the index of the last set bit in the remaining iteration range.
  // precondition: !is_empty()
  // precondition: constructed for reverse iteration.
  idx_t last() const;

  // Updates first() to the position of the first set bit in the range
  // [first() + 1, last()]. The iterator instead becomes empty if there
  // aren't any set bits in that range.
  // precondition: !is_empty()
  // precondition: constructed for forward iteration.
  void step_first();

  // Updates last() to the position of the last set bit in the range
  // [first(), last()). The iterator instead becomes empty if there aren't
  // any set bits in that range.
  // precondition: !is_empty()
  // precondition: constructed for reverse iteration.
  void step_last();
};

// Provides iteration over the indices of the set bits in a range of a bitmap,
// in increasing order. This is an alternative to the iterate() function.
class BitMap::Iterator {
  IteratorImpl _impl;

public:
  // Constructs an empty iterator.
  Iterator();

  // Constructs an iterator for map, over the range [0, map.size()).
  explicit Iterator(const BitMap& map);

  // Constructs an iterator for map, over the range [beg, end).
  // If there are no set bits in that range, the resulting iterator is empty.
  // Otherwise, index() is initially the position of the first set bit in
  // that range.
  // precondition: beg and end form a valid range for map.
  Iterator(const BitMap& map, idx_t beg, idx_t end);

  // Returns true if the remaining iteration range is empty.
  bool is_empty() const;

  // Returns the index of the first set bit in the remaining iteration range.
  // precondition: !is_empty()
  idx_t index() const;

  // Updates index() to the position of the first set bit in the range
  // [index(), end), where end was the corresponding constructor argument.
  // The iterator instead becomes empty if there aren't any set bits in
  // that range.
  // precondition: !is_empty()
  void step();

  // Range-based for loop support.
  RBFIterator begin() const;
  RBFIterator end() const;
};

// Provides iteration over the indices of the set bits in a range of a bitmap,
// in decreasing order. This is an alternative to the reverse_iterate() function.
class BitMap::ReverseIterator {
  IteratorImpl _impl;

  static idx_t initial_end(const BitMap& map, idx_t beg, idx_t end);

public:
  // Constructs an empty iterator.
  ReverseIterator();

  // Constructs a reverse iterator for map, over the range [0, map.size()).
  explicit ReverseIterator(const BitMap& map);

  // Constructs a reverse iterator for map, over the range [beg, end).
  // If there are no set bits in that range, the resulting iterator is empty.
  // Otherwise, index() is initially the position of the last set bit in
  // that range.
  // precondition: beg and end form a valid range for map.
  ReverseIterator(const BitMap& map, idx_t beg, idx_t end);

  // Returns true if the remaining iteration range is empty.
  bool is_empty() const;

  // Returns the index of the last set bit in the remaining iteration range.
  // precondition: !is_empty()
  idx_t index() const;

  // Updates index() to the position of the last set bit in the range
  // [beg, index()), where beg was the corresponding constructor argument.
  // The iterator instead becomes empty if there aren't any set bits in
  // that range.
  // precondition: !is_empty()
  void step();

  // Range-based for loop support.
  ReverseRBFIterator begin() const;
  ReverseRBFIterator end() const;
};

// Provides range-based for loop iteration support.  This class is not
// intended for direct use by an application.  It provides the functionality
// required by a range-based for loop with an Iterator as the range.
class BitMap::RBFIterator {
  friend class Iterator;

  IteratorImpl _impl;

  RBFIterator(const BitMap* map, idx_t beg, idx_t end);

public:
  bool operator!=(const RBFIterator& i) const;
  idx_t operator*() const;
  RBFIterator& operator++();
};

// Provides range-based for loop reverse iteration support.  This class is
// not intended for direct use by an application.  It provides the
// functionality required by a range-based for loop with a ReverseIterator
// as the range.
class BitMap::ReverseRBFIterator {
  friend class ReverseIterator;

  IteratorImpl _impl;

  ReverseRBFIterator(const BitMap* map, idx_t beg, idx_t end);

public:
  bool operator!=(const ReverseRBFIterator& i) const;
  idx_t operator*() const;
  ReverseRBFIterator& operator++();
};

// CRTP: BitmapWithAllocator exposes the following Allocator interfaces upward to GrowableBitMap.
//
//  bm_word_t* allocate(idx_t size_in_words) const;
//  void free(bm_word_t* map, idx_t size_in_words) const
//
template <class BitMapWithAllocator>
class GrowableBitMap : public BitMap {
 protected:
  GrowableBitMap() : GrowableBitMap(nullptr, 0) {}
  GrowableBitMap(bm_word_t* map, idx_t size_in_bits) : BitMap(map, size_in_bits) {}

 private:
  // Copy the region [start, end) of the bitmap
  // Bits in the selected range are copied to a newly allocated map
  bm_word_t* copy_of_range(idx_t start_bit, idx_t end_bit);

 public:
  // Set up and optionally clear the bitmap memory.
  //
  // Precondition: The bitmap was default constructed and has
  // not yet had memory allocated via resize or (re)initialize.
  void initialize(idx_t size_in_bits, bool clear = true);

  // Set up and optionally clear the bitmap memory.
  //
  // Can be called on previously initialized bitmaps.
  void reinitialize(idx_t new_size_in_bits, bool clear = true);

  // Protected functions, that are used by BitMap sub-classes that support them.

  // Resize the backing bitmap memory.
  //
  // Old bits are transferred to the new memory
  // and the extended memory is optionally cleared.
  void resize(idx_t new_size_in_bits, bool clear = true);
  // Reduce bitmap to the region [start, end)
  // Previous map is deallocated and replaced with the newly allocated map from copy_of_range
  void truncate(idx_t start_bit, idx_t end_bit);
};

// A concrete implementation of the "abstract" BitMap class.
//
// The BitMapView is used when the backing storage is managed externally.
class BitMapView : public BitMap {
 public:
  BitMapView() : BitMapView(nullptr, 0) {}
  BitMapView(bm_word_t* map, idx_t size_in_bits) : BitMap(map, size_in_bits) {}
};

// A BitMap with storage in a specific Arena.
class ArenaBitMap : public GrowableBitMap<ArenaBitMap> {
  Arena* const _arena;

  NONCOPYABLE(ArenaBitMap);

 public:
  ArenaBitMap(Arena* arena, idx_t size_in_bits, bool clear = true);

  bm_word_t* allocate(idx_t size_in_words) const;
  bm_word_t* reallocate(bm_word_t* old_map, size_t old_size_in_words, size_t new_size_in_words) const;
  void free(bm_word_t* map, idx_t size_in_words) const {
    // ArenaBitMaps don't free memory.
  }
};

// A BitMap with storage in the current threads resource area.
class ResourceBitMap : public GrowableBitMap<ResourceBitMap> {
 public:
  ResourceBitMap() : ResourceBitMap(0) {}
  explicit ResourceBitMap(idx_t size_in_bits, bool clear = true);

  bm_word_t* allocate(idx_t size_in_words) const;
  bm_word_t* reallocate(bm_word_t* old_map, size_t old_size_in_words, size_t new_size_in_words) const;
  void free(bm_word_t* map, idx_t size_in_words) const {
    // ResourceBitMaps don't free memory.
  }
};

// A BitMap with storage in the CHeap.
class CHeapBitMap : public GrowableBitMap<CHeapBitMap> {
  // NMT memory tag
  const MemTag _mem_tag;

  // Don't allow copy or assignment, to prevent the
  // allocated memory from leaking out to other instances.
  NONCOPYABLE(CHeapBitMap);

 public:
  explicit CHeapBitMap(MemTag mem_tag) : GrowableBitMap(), _mem_tag(mem_tag) {}
  CHeapBitMap(idx_t size_in_bits, MemTag mem_tag, bool clear = true);
  ~CHeapBitMap();

  bm_word_t* allocate(idx_t size_in_words) const;
  bm_word_t* reallocate(bm_word_t* old_map, size_t old_size_in_words, size_t new_size_in_words) const;
  void free(bm_word_t* map, idx_t size_in_words) const;
};

// Convenience class wrapping BitMap which provides multiple bits per slot.
class BitMap2D {
 public:
  typedef BitMap::idx_t idx_t;          // Type used for bit and word indices.
  typedef BitMap::bm_word_t bm_word_t;  // Element type of array that
                                        // represents the bitmap.
 private:
  ResourceBitMap _map;
  idx_t          _bits_per_slot;

  idx_t bit_index(idx_t slot_index, idx_t bit_within_slot_index) const {
    return slot_index * _bits_per_slot + bit_within_slot_index;
  }

  void verify_bit_within_slot_index(idx_t index) const {
    assert(index < _bits_per_slot, "bit_within_slot index out of bounds");
  }

 public:
  // Construction. bits_per_slot must be greater than 0.
  BitMap2D(idx_t bits_per_slot) :
      _map(), _bits_per_slot(bits_per_slot) {}

  // Allocates necessary data structure in resource area. bits_per_slot must be greater than 0.
  BitMap2D(idx_t size_in_slots, idx_t bits_per_slot) :
      _map(size_in_slots * bits_per_slot), _bits_per_slot(bits_per_slot) {}

  idx_t size_in_bits() {
    return _map.size();
  }

  bool is_valid_index(idx_t slot_index, idx_t bit_within_slot_index);
  bool at(idx_t slot_index, idx_t bit_within_slot_index) const;
  void set_bit(idx_t slot_index, idx_t bit_within_slot_index);
  void clear_bit(idx_t slot_index, idx_t bit_within_slot_index);
  void at_put(idx_t slot_index, idx_t bit_within_slot_index, bool value);
  void at_put_grow(idx_t slot_index, idx_t bit_within_slot_index, bool value);
};

// Closure for iterating over BitMaps

class BitMapClosure {
 public:
  // Callback when bit in map is set.  Should normally return "true";
  // return of false indicates that the bitmap iteration should terminate.
  virtual bool do_bit(BitMap::idx_t index) = 0;
};

#endif // SHARE_UTILITIES_BITMAP_HPP
