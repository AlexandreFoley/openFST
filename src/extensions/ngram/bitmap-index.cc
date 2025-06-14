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

#include <fst/extensions/ngram/bitmap-index.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <fst/log.h>
#include <fst/extensions/ngram/nthbit.h>

namespace fst {

static_assert(sizeof(long long) >= sizeof(uint64_t),  // NOLINT
              "__builtin_...ll is used on uint64_t values.");

constexpr std::array<uint64_t, 64> LowBitsMasks() {
  std::array<uint64_t, 64> m{};
  for (int i = 0; i < 64; ++i) m[i] = (uint64_t{1} << i) - 1;
  return m;
}
constexpr std::array<uint64_t, 64> kLowBitsMasks = LowBitsMasks();

size_t BitmapIndex::Rank1(size_t end) const {
  DFST_CHECK_LE(end, Bits());
  // TODO(jrosenstock): Remove nullptr support and this special case.
  if (end == 0) return 0;
  // Without this special case, we'd go past the end. It's questionable
  // whether we should support end == Bits().
  if (end >= num_bits_) return GetOnesCount();
  const uint32_t end_word = end / kStorageBitSize;
  const uint32_t sum = GetIndexOnesCount(end_word);
  const int bit_index = end % kStorageBitSize;
  // TODO(jrosenstock): better with or without special case, and does
  // this depend on whether there's a popcnt instruction?
  if (bit_index == 0) return sum;  // Entire answer is in the index.
  return sum + __builtin_popcountll(bits_[end_word] & kLowBitsMasks[bit_index]);
}

size_t BitmapIndex::Select1(size_t bit_index) const {
  if (bit_index >= GetOnesCount()) return Bits();
  const RankIndexEntry& entry = FindRankIndexEntry(bit_index);
  const uint32_t block_index = &entry - rank_index_.data();
  // TODO(jrosenstock): Look at whether word or bit indices are faster.
  static_assert(kUnitsPerRankIndexEntry == 8);
  uint32_t word_index = block_index * kUnitsPerRankIndexEntry;

  // Find position within this block.
  uint32_t rembits = bit_index - entry.absolute_ones_count();
  if (rembits < entry.relative_ones_count_4()) {
    if (rembits < entry.relative_ones_count_2()) {
      if (rembits < entry.relative_ones_count_1()) {
        // First word, nothing to do.
      } else {
        word_index += 1;
        rembits -= entry.relative_ones_count_1();
      }
    } else if (rembits < entry.relative_ones_count_3()) {
      word_index += 2;
      rembits -= entry.relative_ones_count_2();
    } else {
      word_index += 3;
      rembits -= entry.relative_ones_count_3();
    }
  } else if (rembits < entry.relative_ones_count_6()) {
    if (rembits < entry.relative_ones_count_5()) {
      word_index += 4;
      rembits -= entry.relative_ones_count_4();
    } else {
      word_index += 5;
      rembits -= entry.relative_ones_count_5();
    }
  } else if (rembits < entry.relative_ones_count_7()) {
    word_index += 6;
    rembits -= entry.relative_ones_count_6();
  } else {
    word_index += 7;
    rembits -= entry.relative_ones_count_7();
  }

  const int nth = nth_bit(bits_[word_index], rembits);
  return kStorageBitSize * word_index + nth;
}

size_t BitmapIndex::Select0(size_t bit_index) const {
  const uint32_t zeros_count = Bits() - GetOnesCount();
  if (bit_index >= zeros_count) return Bits();
  const RankIndexEntry& entry = FindInvertedRankIndexEntry(bit_index);
  const uint32_t block_index = &entry - rank_index_.data();
  static_assert(kUnitsPerRankIndexEntry == 8);
  uint32_t word_index = block_index * kUnitsPerRankIndexEntry;

  // Find position within this block.
  uint32_t entry_zeros_count =
      kStorageBitSize * word_index - entry.absolute_ones_count();
  uint32_t remzeros = bit_index - entry_zeros_count;
  if (remzeros < 4 * kStorageBitSize - entry.relative_ones_count_4()) {
    if (remzeros < 2 * kStorageBitSize - entry.relative_ones_count_2()) {
      if (remzeros < kStorageBitSize - entry.relative_ones_count_1()) {
        // Nothing to do.
      } else {
        word_index += 1;
        remzeros -= kStorageBitSize - entry.relative_ones_count_1();
      }
    } else if (remzeros < 3 * kStorageBitSize - entry.relative_ones_count_3()) {
      word_index += 2;
      remzeros -= 2 * kStorageBitSize - entry.relative_ones_count_2();
    } else {
      word_index += 3;
      remzeros -= 3 * kStorageBitSize - entry.relative_ones_count_3();
    }
  } else if (remzeros < 6 * kStorageBitSize - entry.relative_ones_count_6()) {
    if (remzeros < 5 * kStorageBitSize - entry.relative_ones_count_5()) {
      word_index += 4;
      remzeros -= 4 * kStorageBitSize - entry.relative_ones_count_4();
    } else {
      word_index += 5;
      remzeros -= 5 * kStorageBitSize - entry.relative_ones_count_5();
    }
  } else if (remzeros < 7 * kStorageBitSize - entry.relative_ones_count_7()) {
    word_index += 6;
    remzeros -= 6 * kStorageBitSize - entry.relative_ones_count_6();
  } else {
    word_index += 7;
    remzeros -= 7 * kStorageBitSize - entry.relative_ones_count_7();
  }

  const int nth = nth_bit(~bits_[word_index], remzeros);
  return kStorageBitSize * word_index + nth;
}

std::pair<size_t, size_t> BitmapIndex::Select0s(size_t bit_index) const {
  const uint32_t zeros_count = Bits() - GetOnesCount();
  if (bit_index >= zeros_count) return {Bits(), Bits()};
  if (bit_index + 1 >= zeros_count) return {Select0(bit_index), Bits()};

  const RankIndexEntry& entry = FindInvertedRankIndexEntry(bit_index);
  const uint32_t block_index = &entry - rank_index_.data();
  uint32_t word_index = block_index * kUnitsPerRankIndexEntry;

  // Find position within this block.
  uint32_t entry_zeros_count =
      kStorageBitSize * word_index - entry.absolute_ones_count();
  uint32_t remzeros = bit_index - entry_zeros_count;
  if (remzeros < 4 * kStorageBitSize - entry.relative_ones_count_4()) {
    if (remzeros < 2 * kStorageBitSize - entry.relative_ones_count_2()) {
      if (remzeros < kStorageBitSize - entry.relative_ones_count_1()) {
        // Nothing to do.
      } else {
        word_index += 1;
        remzeros -= kStorageBitSize - entry.relative_ones_count_1();
      }
    } else if (remzeros < 3 * kStorageBitSize - entry.relative_ones_count_3()) {
      word_index += 2;
      remzeros -= 2 * kStorageBitSize - entry.relative_ones_count_2();
    } else {
      word_index += 3;
      remzeros -= 3 * kStorageBitSize - entry.relative_ones_count_3();
    }
  } else if (remzeros < 6 * kStorageBitSize - entry.relative_ones_count_6()) {
    if (remzeros < 5 * kStorageBitSize - entry.relative_ones_count_5()) {
      word_index += 4;
      remzeros -= 4 * kStorageBitSize - entry.relative_ones_count_4();
    } else {
      word_index += 5;
      remzeros -= 5 * kStorageBitSize - entry.relative_ones_count_5();
    }
  } else if (remzeros < 7 * kStorageBitSize - entry.relative_ones_count_7()) {
    word_index += 6;
    remzeros -= 6 * kStorageBitSize - entry.relative_ones_count_6();
  } else {
    word_index += 7;
    remzeros -= 7 * kStorageBitSize - entry.relative_ones_count_7();
  }

  // Find the position of the bit_index-th zero.
  const uint64_t inv_word = ~bits_[word_index];
  const int nth = nth_bit(inv_word, remzeros);

  // Then, we want to "1-out" everything below that position, and count trailing
  // ones on the result. This gives us the position of the next zero.
  // There is no count trailing ones builtin, so we invert and use count
  // trailing zeros.

  // This mask has 1s in the nth+1 low order bits; it is equivalent to
  // (1 << (nth + 1)) - 1, but doesn't need a special case when nth == 63.
  // We want ~0 in this case anyway. We want nth+1 because if the bit_index-th
  // zero is in position nth, we need to skip nth+1 positions.
  const uint64_t mask = -(uint64_t{0x2} << nth);  // == ~((2 << nth) - 1)
  const uint64_t masked_inv_word = inv_word & mask;

  // If this is 0, then the next zero is not in the same word.
  if (masked_inv_word != 0) {
    // We can't ctz on 0, but we already checked that.
    const int next_nth = __builtin_ctzll(masked_inv_word);
    return {kStorageBitSize * word_index + nth,
            kStorageBitSize * word_index + next_nth};
  } else {
    // TODO(jrosenstock): Try other words in the block.
    // This should not be massively important. With a bit density of 1/2,
    // 31/32 zeros in a word have the next zero in the same word.
    return {kStorageBitSize * word_index + nth, Select0(bit_index + 1)};
  }
}

uint32_t BitmapIndex::GetIndexOnesCount(size_t array_index) const {
  const auto& rank_index_entry =
      rank_index_[array_index / kUnitsPerRankIndexEntry];
  uint32_t ones_count = rank_index_entry.absolute_ones_count();
  static_assert(kUnitsPerRankIndexEntry == 8);
  return ones_count + rank_index_entry.relative_ones_count(
                          array_index % kUnitsPerRankIndexEntry);
}

void BitmapIndex::BuildIndex(const uint64_t* bits, size_t num_bits,
                             bool enable_select_0_index,
                             bool enable_select_1_index) {
  // Absolute counts are uint32s, so this is the most *set* bits we support
  // for now. Just check the number of *input* bits is less than this
  // to keep things simple.
  DFST_CHECK_LT(num_bits, uint64_t{1} << 32);
  bits_ = bits;
  num_bits_ = num_bits;
  rank_index_.clear();
  rank_index_.resize(rank_index_size());

  select_0_index_.clear();
  if (enable_select_0_index) {
    // Reserve approximately enough for density = 1/2.
    select_0_index_.reserve(num_bits / (2 * kBitsPerSelect0Block) + 1);
  }

  select_1_index_.clear();
  if (enable_select_1_index) {
    select_1_index_.reserve(num_bits / (2 * kBitsPerSelect1Block) + 1);
  }

  const size_t kArraySize = ArraySize();
  uint32_t ones_count = 0;
  uint32_t zeros_count = 0;
  for (uint32_t word_index = 0; word_index < kArraySize; word_index += 8) {
    const uint64_t word[8] = {
        bits[word_index],
        (word_index + 1 < kArraySize) ? bits[word_index + 1] : 0,
        (word_index + 2 < kArraySize) ? bits[word_index + 2] : 0,
        (word_index + 3 < kArraySize) ? bits[word_index + 3] : 0,
        (word_index + 4 < kArraySize) ? bits[word_index + 4] : 0,
        (word_index + 5 < kArraySize) ? bits[word_index + 5] : 0,
        (word_index + 6 < kArraySize) ? bits[word_index + 6] : 0,
        (word_index + 7 < kArraySize) ? bits[word_index + 7] : 0,
    };

    const int word_ones_count[8] = {
        __builtin_popcountll(word[0]), __builtin_popcountll(word[1]),
        __builtin_popcountll(word[2]), __builtin_popcountll(word[3]),
        __builtin_popcountll(word[4]), __builtin_popcountll(word[5]),
        __builtin_popcountll(word[6]), __builtin_popcountll(word[7]),
    };

    auto& rank_index_entry = rank_index_[word_index / kUnitsPerRankIndexEntry];
    const uint32_t abs_ones_count = ones_count;
    rank_index_entry.set_absolute_ones_count(abs_ones_count);
    ones_count += word_ones_count[0];
    rank_index_entry.set_relative_ones_count_1(ones_count - abs_ones_count);
    ones_count += word_ones_count[1];
    rank_index_entry.set_relative_ones_count_2(ones_count - abs_ones_count);
    ones_count += word_ones_count[2];
    rank_index_entry.set_relative_ones_count_3(ones_count - abs_ones_count);
    ones_count += word_ones_count[3];
    rank_index_entry.set_relative_ones_count_4(ones_count - abs_ones_count);
    ones_count += word_ones_count[4];
    rank_index_entry.set_relative_ones_count_5(ones_count - abs_ones_count);
    ones_count += word_ones_count[5];
    rank_index_entry.set_relative_ones_count_6(ones_count - abs_ones_count);
    ones_count += word_ones_count[6];
    rank_index_entry.set_relative_ones_count_7(ones_count - abs_ones_count);
    ones_count += word_ones_count[7];

    if (enable_select_0_index) {
      int s0_zeros_count = zeros_count;
      for (int i = 0; i < 8; ++i) {
        const size_t bit_offset = (word_index + i) * kStorageBitSize;
        if (bit_offset >= num_bits_) break;

        // Zeros count is somewhat more involved to compute, so only do it
        // if we need it. The last word has zeros in the high bits, so
        // that needs to be accounted for when computing the zeros count
        // from the ones count.
        const uint32_t bits_remaining = num_bits - bit_offset;
        const int word_zeros_count =
            std::min(bits_remaining, kStorageBitSize) - word_ones_count[i];

        // We record a 0 every kBitsPerSelect0Block bits. So, if zeros_count
        // is 0 mod kBitsPerSelect0Block, we record the next zero. If
        // zeros_count is 1 mod kBitsPerSelect0Block, we need to skip
        // kBitsPerSelect0Block - 1 zeros, then record a zero. And so on.
        // What function is this?  It's -zeros_count % kBitsPerSelect0Block.
        const uint32_t zeros_to_skip = -s0_zeros_count % kBitsPerSelect0Block;
        if (word_zeros_count > zeros_to_skip) {
          const int nth = nth_bit(~word[i], zeros_to_skip);
          select_0_index_.push_back(bit_offset + nth);
          static_assert(kBitsPerSelect0Block >= 512,
                        "kBitsPerSelect0Block must be at least 512.");
          break;  // 8 entries is 512 bits, so we can't push another bit here.
        }
        s0_zeros_count += word_zeros_count;
      }
      zeros_count += 8 * kStorageBitSize - (ones_count - abs_ones_count);
    }

    if (enable_select_1_index) {
      int s1_ones_count = abs_ones_count;
      for (int i = 0; i < 8; ++i) {
        const size_t bit_offset = (word_index + i) * kStorageBitSize;
        uint32_t ones_to_skip = -s1_ones_count % kBitsPerSelect1Block;
        if (word_ones_count[i] > ones_to_skip) {
          const int nth = nth_bit(word[i], ones_to_skip);
          select_1_index_.push_back(bit_offset + nth);
          static_assert(kBitsPerSelect1Block >= 512,
                        "kBitsPerSelect1Block must be at least 512.");
          break;  // 8 entries is 512 bits, so we can't push another bit here.
        }
        s1_ones_count += word_ones_count[i];
      }
    }
  }
  // Add the extra entry with the total number of bits.
  rank_index_.back().set_absolute_ones_count(ones_count);

  if (enable_select_0_index) {
    // Add extra entry with num_bits_.
    select_0_index_.push_back(num_bits_);
    select_0_index_.shrink_to_fit();
  }

  if (enable_select_1_index) {
    select_1_index_.push_back(num_bits_);
    select_1_index_.shrink_to_fit();
  }
}

const BitmapIndex::RankIndexEntry& BitmapIndex::FindRankIndexEntry(
    size_t bit_index) const {
  DFST_CHECK_GE(bit_index, 0);
  DFST_CHECK_LT(bit_index, rank_index_.back().absolute_ones_count());

  const RankIndexEntry* begin = nullptr;
  const RankIndexEntry* end = nullptr;
  if (select_1_index_.empty()) {
    begin = &rank_index_[0];
    end = begin + rank_index_.size();
  } else {
    const uint32_t select_index = bit_index / kBitsPerSelect1Block;
    DFST_CHECK_LT(select_index + 1, select_1_index_.size());

    // TODO(jrosenstock): It would be nice to handle the exact hit
    // bit_index % kBitsPerSelect1Block == 0 case so we could
    // return the value, but that requiries some refactoring:
    // either inlining this into Select1, or returning a pair
    // or out param, etc.

    // The bit is between these indices.
    const uint32_t lo_bit_index = select_1_index_[select_index];
    const uint32_t hi_bit_index = select_1_index_[select_index + 1];

    begin = &rank_index_[lo_bit_index / kBitsPerSelect1Block];
    end = &rank_index_[(hi_bit_index + kBitsPerSelect1Block - 1) /
                       kBitsPerSelect1Block];
  }

  // Linear search if the range is small.
  const RankIndexEntry* entry = nullptr;
  if (end - begin <= kMaxLinearSearchBlocks) {
    for (entry = begin; entry != end; ++entry) {
      if (entry->absolute_ones_count() > bit_index) break;
    }
  } else {
    RankIndexEntry search_entry;
    search_entry.set_absolute_ones_count(bit_index);
    // TODO(jrosenstock): benchmark upper vs custom bsearch.
    entry = &*std::upper_bound(
        begin, end, search_entry,
        [](const RankIndexEntry& e1, const RankIndexEntry& e2) {
          return e1.absolute_ones_count() < e2.absolute_ones_count();
        });
  }

  const auto& e = *(entry - 1);
  DFST_CHECK_LE(e.absolute_ones_count(), bit_index);
  DFST_CHECK_GT(entry->absolute_ones_count(), bit_index);
  return e;
}

const BitmapIndex::RankIndexEntry& BitmapIndex::FindInvertedRankIndexEntry(
    size_t bit_index) const {
  DFST_CHECK_GE(bit_index, 0);
  DFST_CHECK_LT(bit_index, num_bits_ - rank_index_.back().absolute_ones_count());

  uint32_t lo = 0, hi = 0;
  if (select_0_index_.empty()) {
    lo = 0;
    hi = (num_bits_ + kBitsPerRankIndexEntry - 1) / kBitsPerRankIndexEntry;
  } else {
    const uint32_t select_index = bit_index / kBitsPerSelect0Block;
    DFST_CHECK_LT(select_index + 1, select_0_index_.size());

    // TODO(jrosenstock): Same special case for exact hit.

    lo = select_0_index_[select_index] / kBitsPerSelect0Block;
    hi = (select_0_index_[select_index + 1] + kBitsPerSelect0Block - 1) /
         kBitsPerSelect0Block;
  }

  DFST_CHECK_LT(hi, rank_index_.size());
  // Linear search never showed an advantage when benchmarking. This may be
  // because the linear search is more complex with the zeros_count computation,
  // or because the ranges are larger, so linear search is triggered less often,
  // and the difference is harder to measure.
  while (lo + 1 < hi) {
    const uint32_t mid = lo + (hi - lo) / 2;
    if (bit_index <
        kBitsPerRankIndexEntry * mid - rank_index_[mid].absolute_ones_count()) {
      hi = mid;
    } else {
      lo = mid;
    }
  }

  DFST_CHECK_LE(lo * kBitsPerRankIndexEntry - rank_index_[lo].absolute_ones_count(),
            bit_index);
  if ((lo + 1) * kBitsPerRankIndexEntry <= num_bits_) {
    DFST_CHECK_GT((lo + 1) * kBitsPerRankIndexEntry -
                  rank_index_[lo + 1].absolute_ones_count(),
              bit_index);
  } else {
    DFST_CHECK_GT(num_bits_ - rank_index_[lo + 1].absolute_ones_count(), bit_index);
  }
  return rank_index_[lo];
}

}  // end namespace fst
