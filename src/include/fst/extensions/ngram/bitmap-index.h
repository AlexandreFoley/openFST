// Copyright 2005-2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the 'License');
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an 'AS IS' BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.

#ifndef FST_EXTENSIONS_NGRAM_BITMAP_INDEX_H_
#define FST_EXTENSIONS_NGRAM_BITMAP_INDEX_H_

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <fst/compat.h>
#include <fst/log.h>

// This class is a bitstring storage class with an index that allows
// seeking to the Nth set or clear bit in time O(Log(N)) (or
// O(log(1/density) if the relevant select index is enabled) where N is the
// length of the bit vector, and density is the block density of zeros/ones
// (for Select0/Select1 respectively). The block density is for block i is
// B / span_i, where B is the block size in bits (512); and span_i is
// Select(B * (i + 1)) - Select(B * i), the range of the bitstring that
// select index block i indexes. That is, B 0 (or 1) bits occur over
// span_i bits of the bit string.
//
// To turn this into the "standard"constant time select, there would need
// to be a span size threshold. Block spanning more than this would need
// to have the position of each bit explicitly recorded. 8k is a typical
// value for this threshold, but I saw no spans larger than ~6k.
//
// In addition, this class allows counting set or clear bits over ranges in
// constant time.
//
// This is accomplished by maintaining an index of the running popcounts
// of the bitstring. The index is divided into blocks that cover the
// size of a cache line (8 64-bit words). Each entry has one absolute count
// of all the 1s that appear before the block and 7 relative counts since the
// beginning of the block.
//
// The bitstring itself is stored as uint64s:
// uint64_t *bits_;
//
// The rank index looks like
// struct RankIndexEntry {
//   uint32_t absolute_ones_count();
//   uint32_t relative_ones_count_1();
//   ...
//   uint32_t relative_ones_count_7();
// };
// vector<RankIndexEntry> rank_index_;
//
// Where rank_index_[i].absolute_ones_count() == Rank1(512 * i), and
// for k in 1 .. 7:
// rank_index_[i].relative_ones_count_k() ==
//     Rank1(512 * i + 64 * k) - Rank1(512 * i).
//
// This index is queried directly for Rank0 and Rank1 and binary searched
// for Select0 and Select1. If configured in the constructor or via
// BuildIndex, additional indices for Select0 and Select1 can be built
// to reduce these operations to O(log(1/density)) as explained above.
//
// The select indexes are stored as
// vector<uint32_t> select_0_index_;
// where
// select_0_index_[i] == Select0(512 * i).
// Similarly for select_1_index_.
//
// To save space, the absolute counts are stored as uint32_t. Therefore,
// only bitstrings with < 2**32 ones are supported.
//
// For each 64 bytes of input (8 8-byte words) there are 12 bytes of index
// (4 bytes for the absolute count and 2 * 4 bytes for the relative counts)
// for a 18.75% space overhead.
//
// The select indices have 6.25% overhead together.

namespace fst {

class BitmapIndex {
 public:
  static size_t StorageSize(size_t num_bits) {
    return ((num_bits + kStorageBlockMask) >> kStorageLogBitSize);
  }

  BitmapIndex() = default;
  BitmapIndex(BitmapIndex&&) = default;
  BitmapIndex& operator=(BitmapIndex&&) = default;

  // Convenience constructor to avoid a separate BuildIndex call.
  BitmapIndex(const uint64_t* bits, std::size_t num_bits,
              bool enable_select_0_index = false,
              bool enable_select_1_index = false) {
    BuildIndex(bits, num_bits, enable_select_0_index, enable_select_1_index);
  }

  bool Get(size_t index) const { return Get(bits_, index); }

  static bool Get(const uint64_t* bits, size_t index) {
    return (bits[index >> kStorageLogBitSize] &
            (kOne << (index & kStorageBlockMask))) != 0;
  }

  static void Set(uint64_t* bits, size_t index) {
    bits[index >> kStorageLogBitSize] |= (kOne << (index & kStorageBlockMask));
  }

  static void Clear(uint64_t* bits, size_t index) {
    bits[index >> kStorageLogBitSize] &= ~(kOne << (index & kStorageBlockMask));
  }

  size_t Bits() const { return num_bits_; }

  size_t ArraySize() const { return StorageSize(num_bits_); }

  // Number of bytes used to store the bit vector.
  size_t ArrayBytes() const { return ArraySize() * sizeof(bits_[0]); }

  // Number of bytes used to store the rank index.
  size_t IndexBytes() const {
    return (rank_index_.size() * sizeof(rank_index_[0]) +
            select_0_index_.size() * sizeof(select_0_index_[0]) +
            select_1_index_.size() * sizeof(select_1_index_[0]));
  }

  // Returns the number of one bits in the bitmap
  size_t GetOnesCount() const {
    // We keep an extra entry with the total count.
    return rank_index_.back().absolute_ones_count();
  }

  // Returns the number of one bits in positions 0 to limit - 1.
  // REQUIRES: limit <= Bits()
  size_t Rank1(size_t end) const;

  // Returns the number of zero bits in positions 0 to limit - 1.
  // REQUIRES: limit <= Bits()
  size_t Rank0(size_t end) const { return end - Rank1(end); }

  // Returns the offset to the nth set bit (zero based)
  // or Bits() if index >= number of ones
  size_t Select1(size_t bit_index) const;

  // Returns the offset to the nth clear bit (zero based)
  // or Bits() if index > number of
  size_t Select0(size_t bit_index) const;

  // Returns the offset of the nth and nth+1 clear bit (zero based),
  // equivalent to two calls to Select0, but more efficient.
  std::pair<size_t, size_t> Select0s(size_t bit_index) const;

  // Rebuilds from index for the associated Bitmap, should be called
  // whenever changes have been made to the Bitmap or else behavior
  // of the indexed bitmap methods will be undefined.
  void BuildIndex(const uint64_t* bits, size_t num_bits,
                  bool enable_select_0_index = false,
                  bool enable_select_1_index = false);

  static constexpr uint64_t kOne = 1;
  static constexpr uint32_t kStorageBitSize = 64;
  static constexpr uint32_t kStorageLogBitSize = 6;

 private:
  static constexpr uint32_t kUnitsPerRankIndexEntry = 8;
  static constexpr uint32_t kBitsPerRankIndexEntry =
      kUnitsPerRankIndexEntry * kStorageBitSize;
  static constexpr uint32_t kStorageBlockMask = kStorageBitSize - 1;

  // TODO(jrosenstock): benchmark different values here.
  // It's reasonable that these are the same since density is typically around
  // 1/2.
  static constexpr uint32_t kBitsPerSelect0Block = 512;
  static constexpr uint32_t kBitsPerSelect1Block = 512;

  // If this many or fewer RankIndexEntry blocks need to be searched by
  // FindRankIndexEntry use a linear search instead of a binary search.
  // FindInvertedRankIndexEntry always uses binary search, since linear
  // search never showed improvements on benchmarks. The value of 8 was
  // faster than smaller values on benchmarks, but I do not feel comfortable
  // raising it because there are very few times a higher value would
  // make a difference. Thus, whether a higher value helps or hurts is harder
  // to measure. TODO(jrosenstock): Try to measure with low bit density.
  static constexpr uint32_t kMaxLinearSearchBlocks = 8;

  // A RankIndexEntry covers a block of 8 64-bit words (one cache line on
  // x86_64 and ARM). It consists of an absolute count of all the 1s that
  // appear before this block, and 7 relative counts for the 1s within
  // the block. relative_ones_count_k = popcount(block[0:k]).
  // The relative counts are stored in bitfields.
  // A RankIndexEntry takes 12 bytes, for 12/64 = 18.75% overhead.
  // See also documentation at the top of the file.
  class RankIndexEntry {
   public:
    RankIndexEntry() = default;

    uint32_t absolute_ones_count() const { return absolute_ones_count_; }

    // Returns the popcounts of words *before* word `k` in the block.
    uint32_t relative_ones_count(size_t k) const {
      assert(k < 8);
      // TODO(jrosenstock): Try a multiply here instead.
      const uint32_t c = k < 4 ? 0 : relative_ones_count_4_;
      // Load starting one byte before the three relative counts we want.
      // This load is unaligned for `k < 4` and aligned otherwise.  The
      // address is always within `RankIndexEntry`.
      uint32_t relative_ones_counts_8x4 =
          UnalignedLoad<uint32_t>(&relative_ones_counts_[k >> 2][0] - 1);
#if ((defined __ORDER_LITTLE_ENDIAN__) || (defined _WIN32))
      // Clear out the garbage byte.
      relative_ones_counts_8x4 &= ~0xFF;
      return c + ((relative_ones_counts_8x4 >> (8 * (k & 3))) & 0xFF);
#else
#error "Big-endian currently unsupported."
#endif
    }

    uint32_t relative_ones_count_1() const {
      return relative_ones_counts_[0][0];
    }
    uint32_t relative_ones_count_2() const {
      return relative_ones_counts_[0][1];
    }
    uint32_t relative_ones_count_3() const {
      return relative_ones_counts_[0][2];
    }
    uint32_t relative_ones_count_4() const { return relative_ones_count_4_; }
    uint32_t relative_ones_count_5() const {
      return relative_ones_count_4() + relative_ones_counts_[1][0];
    }
    uint32_t relative_ones_count_6() const {
      return relative_ones_count_4() + relative_ones_counts_[1][1];
    }
    uint32_t relative_ones_count_7() const {
      return relative_ones_count_4() + relative_ones_counts_[1][2];
    }

    void set_absolute_ones_count(uint32_t v) { absolute_ones_count_ = v; }
    void set_relative_ones_count_1(uint32_t v) {
      DFST_CHECK_LE(v, kStorageBitSize);
      relative_ones_counts_[0][0] = v;
    }
    void set_relative_ones_count_2(uint32_t v) {
      DFST_CHECK_LE(v, 2 * kStorageBitSize);
      relative_ones_counts_[0][1] = v;
    }
    void set_relative_ones_count_3(uint32_t v) {
      DFST_CHECK_LE(v, 3 * kStorageBitSize);
      relative_ones_counts_[0][2] = v;
    }
    void set_relative_ones_count_4(uint32_t v) {
      DFST_CHECK_LE(v, 4 * kStorageBitSize);
      DFST_CHECK_EQ(relative_ones_counts_[1][0], 0);
      DFST_CHECK_EQ(relative_ones_counts_[1][1], 0);
      DFST_CHECK_EQ(relative_ones_counts_[1][2], 0);
      relative_ones_count_4_ = v;
    }
    void set_relative_ones_count_5(uint32_t v) {
      DFST_CHECK_LE(v, 5 * kStorageBitSize);
      relative_ones_counts_[1][0] = v - relative_ones_count_4();
    }
    void set_relative_ones_count_6(uint32_t v) {
      DFST_CHECK_LE(v, 6 * kStorageBitSize);
      relative_ones_counts_[1][1] = v - relative_ones_count_4();
    }
    void set_relative_ones_count_7(uint32_t v) {
      DFST_CHECK_LE(v, 7 * kStorageBitSize);
      relative_ones_counts_[1][2] = v - relative_ones_count_4();
    }

   private:
    // Popcount of 1s before this block.
    // rank_index_[i].absolute_ones_count() == Rank1(512 * i).
    uint32_t absolute_ones_count_ = 0;

    // Popcount of 1s since the beginning of the block.
    // rank_index_[i].relative_ones_count(k) ==
    // rank_index_[i].relative_ones_count_k() ==
    //     Rank1(512 * i + 64 * k) - Rank1(512 * i).
    //
    // Three consecutive uint64s may have a total of at most 3 * 64 = 192 < 256
    // ones set. Thus we store `relative_ones_count_1(), ...
    // relative_ones_count_3()` directly in the one-byte integers
    // `relative_ones_counts_[0][0], ..., relative_ones_counts_[0][2]`. We use
    // 16 bits for relative_ones_count_4_. In `relative_ones_counts_[1][0], ...
    // relative_ones_counts_[1][2]`, we store the difference between
    // `relative_ones_counts_5(), ... `relative_ones_counts_7()` and
    // `relative_ones_count_4_`.  We put `relative_ones_count_4_` in a 16-bit
    // integer because it's often used as the first split point for binary
    // search, so we save an addition there.
    //
    // More explicitly:
    // ```
    // relative_ones_counts_[0][0] = relative_ones_count_1()
    // relative_ones_counts_[0][1] = relative_ones_count_2()
    // relative_ones_counts_[0][2] = relative_ones_count_3()
    // relative_ones_counts_[1][0] =
    //     relative_ones_count_5() - relative_ones_count_4()
    // relative_ones_counts_[1][1] =
    //     relative_ones_count_6() - relative_ones_count_4()
    // relative_ones_counts_[1][2] =
    //     relative_ones_count_7() - relative_ones_count_4()
    // ```
    //
    // (As a consequence, it is an error to call set_relative_ones_count_4()
    // after calling set_relative_ones_count_N() for N in {5, 6, 7}.)
    uint16_t relative_ones_count_4_ = 0;
    uint8_t relative_ones_counts_[2][3] = {{0, 0, 0}, {0, 0, 0}};
  };
  static_assert(sizeof(RankIndexEntry) == 4 + 8,
                "RankIndexEntry should be 12 bytes.");

  // Returns, from the index, the count of ones up to array_index.
  uint32_t GetIndexOnesCount(size_t array_index) const;

  // Finds the entry in the rank index for the block containing the
  // bit_index-th 1 bit.
  const RankIndexEntry& FindRankIndexEntry(size_t bit_index) const;

  // Finds the entry in the rank index for the block containing the
  // bit_index-th 0 bit.
  const RankIndexEntry& FindInvertedRankIndexEntry(size_t bit_index) const;

  // We create a combined primary and secondary index, with one extra entry
  // to hold the total number of bits.
  size_t rank_index_size() const {
    return (ArraySize() + kUnitsPerRankIndexEntry - 1) /
               kUnitsPerRankIndexEntry +
           1;
  }

  const uint64_t* bits_ = nullptr;
  size_t num_bits_ = 0;

  std::vector<RankIndexEntry> rank_index_;

  // Index of positions for Select0
  // select_0_index_[i] == Select0(kBitsPerSelect0Block * i).
  // Empty means there is no index, otherwise, we always add an extra entry
  // with num_bits_. Overhead is 4 bytes / 64 bytes of zeros,
  // so 4/64 times the density of zeros. This is 6.25% * zeros_density.
  std::vector<uint32_t> select_0_index_;

  // Index of positions for Select1
  // select_1_index_[i] == Select1(kBitsPerSelect1Block * i).
  // Empty means there is no index, otherwise, we always add an extra entry
  // with num_bits_. Overhead is 4 bytes / 64 bytes of ones,
  // so 4/64 times the density of ones. This is 6.25% * ones_density.
  std::vector<uint32_t> select_1_index_;
};

}  // end namespace fst

#endif  // FST_EXTENSIONS_NGRAM_BITMAP_INDEX_H_
