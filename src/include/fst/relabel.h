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
//
// Functions and classes to relabel an FST (either on input or output).

#ifndef FST_RELABEL_H_
#define FST_RELABEL_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <fst/log.h>
#include <fst/arc.h>
#include <fst/cache.h>
#include <fst/float-weight.h>
#include <fst/fst.h>
#include <fst/impl-to-fst.h>
#include <fst/mutable-fst.h>
#include <fst/properties.h>
#include <fst/symbol-table.h>
#include <fst/util.h>
#include <unordered_map>

namespace fst {

// Relabels either the input labels or output labels. The old to
// new labels are specified using a vector of std::pair<Label, Label>.
// Any label associations not specified are assumed to be identity
// mapping. The destination labels must be valid labels (e.g., not kNoLabel).
template <class Arc>
void Relabel(
    MutableFst<Arc> *fst,
    const std::vector<std::pair<typename Arc::Label, typename Arc::Label>> &        ipairs,
    const std::vector<std::pair<typename Arc::Label, typename Arc::Label>> &        opairs) {
  using Label = typename Arc::Label;
  const auto props = fst->Properties(kFstProperties, false);
  // Constructs label-to-label maps.
  const std::unordered_map<Label, Label> input_map(
      ipairs.begin(), ipairs.end());
  const std::unordered_map<Label, Label> output_map(
      opairs.begin(), opairs.end());
  for (StateIterator<MutableFst<Arc>> siter(*fst); !siter.Done();
       siter.Next()) {
    for (MutableArcIterator<MutableFst<Arc>> aiter(fst, siter.Value());
         !aiter.Done(); aiter.Next()) {
      auto arc = aiter.Value();
      // dense_hash_map does not support find on the empty_key_val.
      // These labels should never be in an FST anyway.
      DFST_CHECK_NE(arc.ilabel, kNoLabel);
      DFST_CHECK_NE(arc.olabel, kNoLabel);
      // Relabels input.
      if (auto it = input_map.find(arc.ilabel); it != input_map.end()) {
        if (it->second == kNoLabel) {
          FSTERROR() << "Input symbol ID " << arc.ilabel
                     << " missing from target vocabulary";
          fst->SetProperties(kError, kError);
          return;
        }
        arc.ilabel = it->second;
      }
      // Relabels output.
      if (auto it = output_map.find(arc.olabel); it != output_map.end()) {
        if (it->second == kNoLabel) {
          FSTERROR() << "Output symbol id " << arc.olabel
                     << " missing from target vocabulary";
          fst->SetProperties(kError, kError);
          return;
        }
        arc.olabel = it->second;
      }
      aiter.SetValue(arc);
    }
  }
  fst->SetProperties(RelabelProperties(props), kFstProperties);
}

// Relabels either the input labels or output labels. The old to
// new labels are specified using pairs of old and new symbol tables.
// The tables must contain (at least) all labels on the appropriate side of the
// FST. If the 'unknown_i(o)symbol' is non-empty, it is used to label any
// missing symbol in new_i(o)symbols table.
template <class Arc>
void Relabel(MutableFst<Arc> *fst, const SymbolTable *old_isymbols,
             const SymbolTable *new_isymbols,
             const std::string &unknown_isymbol, bool attach_new_isymbols,
             const SymbolTable *old_osymbols, const SymbolTable *new_osymbols,
             const std::string &unknown_osymbol, bool attach_new_osymbols) {
  using Label = typename Arc::Label;
  // Constructs vectors of input-side label pairs.
  std::vector<std::pair<Label, Label>> ipairs;
  if (old_isymbols && new_isymbols) {
    size_t num_missing_syms = 0;
    Label unknown_ilabel = kNoLabel;
    if (!unknown_isymbol.empty()) {
      unknown_ilabel = new_isymbols->Find(unknown_isymbol);
      if (unknown_ilabel == kNoLabel) {
        VLOG(1) << "Input symbol '" << unknown_isymbol
                << "' missing from target symbol table";
        ++num_missing_syms;
      }
    }

    for (const auto &sitem : *old_isymbols) {
      const auto old_index = sitem.Label();
      const auto symbol = sitem.Symbol();
      auto new_index = new_isymbols->Find(symbol);
      if (new_index == kNoLabel) {
        if (unknown_ilabel != kNoLabel) {
          new_index = unknown_ilabel;
        } else {
          VLOG(1) << "Input symbol ID " << old_index << " symbol '" << symbol
                  << "' missing from target symbol table";
          ++num_missing_syms;
        }
      }
      ipairs.emplace_back(old_index, new_index);
    }
    if (num_missing_syms > 0) {
      LOG(WARNING) << "Target symbol table missing: " << num_missing_syms
                   << " input symbols";
    }
    if (attach_new_isymbols) fst->SetInputSymbols(new_isymbols);
  }
  // Constructs vectors of output-side label pairs.
  std::vector<std::pair<Label, Label>> opairs;
  if (old_osymbols && new_osymbols) {
    size_t num_missing_syms = 0;
    Label unknown_olabel = kNoLabel;
    if (!unknown_osymbol.empty()) {
      unknown_olabel = new_osymbols->Find(unknown_osymbol);
      if (unknown_olabel == kNoLabel) {
        VLOG(1) << "Output symbol '" << unknown_osymbol
                << "' missing from target symbol table";
        ++num_missing_syms;
      }
    }
    for (const auto &sitem : *old_osymbols) {
      const auto old_index = sitem.Label();
      const auto symbol = sitem.Symbol();
      auto new_index = new_osymbols->Find(symbol);
      if (new_index == kNoLabel) {
        if (unknown_olabel != kNoLabel) {
          new_index = unknown_olabel;
        } else {
          VLOG(1) << "Output symbol ID " << old_index << " symbol '" << symbol
                  << "' missing from target symbol table";
          ++num_missing_syms;
        }
      }
      opairs.emplace_back(old_index, new_index);
    }
    if (num_missing_syms > 0) {
      LOG(WARNING) << "Target symbol table missing: " << num_missing_syms
                   << " output symbols";
    }
    if (attach_new_osymbols) fst->SetOutputSymbols(new_osymbols);
  }
  // Calls relabel using vector of relabel pairs.
  Relabel(fst, ipairs, opairs);
}

// Same as previous but no special allowance for unknown symbols. Kept
// for backward compat.
template <class Arc>
void Relabel(MutableFst<Arc> *fst, const SymbolTable *old_isymbols,
             const SymbolTable *new_isymbols, bool attach_new_isymbols,
             const SymbolTable *old_osymbols, const SymbolTable *new_osymbols,
             bool attach_new_osymbols) {
  Relabel(fst, old_isymbols, new_isymbols, "" /* no unknown isymbol */,
          attach_new_isymbols, old_osymbols, new_osymbols,
          "" /* no unknown osymbol */, attach_new_osymbols);
}

// Relabels either the input labels or output labels. The old to
// new labels are specified using symbol tables. Any label associations not
// specified are assumed to be identity mapping.
template <class Arc>
void Relabel(MutableFst<Arc> *fst, const SymbolTable *new_isymbols,
             const SymbolTable *new_osymbols) {
  Relabel(fst, fst->InputSymbols(), new_isymbols, true, fst->OutputSymbols(),
          new_osymbols, true);
}

using RelabelFstOptions = CacheOptions;

template <class Arc>
class RelabelFst;

namespace internal {

// Relabels an FST from one symbol set to another. Relabeling can either be on
// input or output space. RelabelFst implements a delayed version of the
// relabel. Arcs are relabeled on the fly and not cached; i.e., each request is
// recomputed.
template <class Arc>
class RelabelFstImpl : public CacheImpl<Arc> {
 public:
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  using Store = DefaultCacheStore<Arc>;
  using State = typename Store::State;

  using FstImpl<Arc>::SetType;
  using FstImpl<Arc>::SetProperties;
  using FstImpl<Arc>::WriteHeader;
  using FstImpl<Arc>::SetInputSymbols;
  using FstImpl<Arc>::SetOutputSymbols;

  using CacheImpl<Arc>::PushArc;
  using CacheImpl<Arc>::HasArcs;
  using CacheImpl<Arc>::HasFinal;
  using CacheImpl<Arc>::HasStart;
  using CacheImpl<Arc>::SetArcs;
  using CacheImpl<Arc>::SetFinal;
  using CacheImpl<Arc>::SetStart;

  friend class StateIterator<RelabelFst<Arc>>;

  RelabelFstImpl(const Fst<Arc> &fst,
                 const std::vector<std::pair<Label, Label>> &ipairs,
                 const std::vector<std::pair<Label, Label>> &opairs,
                 const RelabelFstOptions &opts)
      : CacheImpl<Arc>(opts),
        fst_(fst.Copy()),
        input_map_(ipairs.begin(), ipairs.end()),
        output_map_(opairs.begin(), opairs.end()),
        relabel_input_(!ipairs.empty()),
        relabel_output_(!opairs.empty()) {
    SetProperties(RelabelProperties(fst.Properties(kCopyProperties, false)));
    SetType("relabel");
  }

  RelabelFstImpl(const Fst<Arc> &fst, const SymbolTable *old_isymbols,
                 const SymbolTable *new_isymbols,
                 const SymbolTable *old_osymbols,
                 const SymbolTable *new_osymbols, const RelabelFstOptions &opts)
      : CacheImpl<Arc>(opts),
        fst_(fst.Copy()),
        relabel_input_(false),
        relabel_output_(false) {
    SetType("relabel");
    SetProperties(RelabelProperties(fst.Properties(kCopyProperties, false)));
    SetInputSymbols(old_isymbols);
    SetOutputSymbols(old_osymbols);
    if (old_isymbols && new_isymbols &&
        old_isymbols->LabeledCheckSum() != new_isymbols->LabeledCheckSum()) {
      for (const auto &sitem : *old_isymbols) {
        input_map_[sitem.Label()] = new_isymbols->Find(sitem.Symbol());
      }
      SetInputSymbols(new_isymbols);
      relabel_input_ = true;
    }
    if (old_osymbols && new_osymbols &&
        old_osymbols->LabeledCheckSum() != new_osymbols->LabeledCheckSum()) {
      for (const auto &sitem : *old_osymbols) {
        output_map_[sitem.Label()] = new_osymbols->Find(sitem.Symbol());
      }
      SetOutputSymbols(new_osymbols);
      relabel_output_ = true;
    }
  }

  RelabelFstImpl(const RelabelFstImpl<Arc> &impl)
      : CacheImpl<Arc>(impl),
        fst_(impl.fst_->Copy(true)),
        input_map_(impl.input_map_),
        output_map_(impl.output_map_),
        relabel_input_(impl.relabel_input_),
        relabel_output_(impl.relabel_output_) {
    SetType("relabel");
    SetProperties(impl.Properties(), kCopyProperties);
    SetInputSymbols(impl.InputSymbols());
    SetOutputSymbols(impl.OutputSymbols());
  }

  StateId Start() {
    if (!HasStart()) SetStart(fst_->Start());
    return CacheImpl<Arc>::Start();
  }

  Weight Final(StateId s) {
    if (!HasFinal(s)) SetFinal(s, fst_->Final(s));
    return CacheImpl<Arc>::Final(s);
  }

  size_t NumArcs(StateId s) {
    if (!HasArcs(s)) Expand(s);
    return CacheImpl<Arc>::NumArcs(s);
  }

  size_t NumInputEpsilons(StateId s) {
    if (!HasArcs(s)) Expand(s);
    return CacheImpl<Arc>::NumInputEpsilons(s);
  }

  size_t NumOutputEpsilons(StateId s) {
    if (!HasArcs(s)) Expand(s);
    return CacheImpl<Arc>::NumOutputEpsilons(s);
  }

  uint64_t Properties() const override { return Properties(kFstProperties); }

  // Sets error if found, and returns other FST impl properties.
  uint64_t Properties(uint64_t mask) const override {
    if ((mask & kError) && fst_->Properties(kError, false)) {
      SetProperties(kError, kError);
    }
    return FstImpl<Arc>::Properties(mask);
  }

  void InitArcIterator(StateId s, ArcIteratorData<Arc> *data) {
    if (!HasArcs(s)) Expand(s);
    CacheImpl<Arc>::InitArcIterator(s, data);
  }

  void Expand(StateId s) {
    for (ArcIterator<Fst<Arc>> aiter(*fst_, s); !aiter.Done(); aiter.Next()) {
      auto arc = aiter.Value();
      if (relabel_input_) {
        if (auto it = input_map_.find(arc.ilabel); it != input_map_.end()) {
          arc.ilabel = it->second;
        }
      }
      if (relabel_output_) {
        if (auto it = output_map_.find(arc.olabel); it != output_map_.end()) {
          arc.olabel = it->second;
        }
      }
      PushArc(s, std::move(arc));
    }
    SetArcs(s);
  }

 private:
  std::unique_ptr<const Fst<Arc>> fst_;

  std::unordered_map<Label, Label> input_map_;
  std::unordered_map<Label, Label> output_map_;
  bool relabel_input_;
  bool relabel_output_;
};

}  // namespace internal

// This class attaches interface to implementation and handles
// reference counting, delegating most methods to ImplToFst.
template <class A>
class RelabelFst : public ImplToFst<internal::RelabelFstImpl<A>> {
  using Base = ImplToFst<internal::RelabelFstImpl<A>>;

 public:
  using Arc = A;
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  using Store = DefaultCacheStore<Arc>;
  using State = typename Store::State;
  using typename Base::Impl;

  friend class ArcIterator<RelabelFst<A>>;
  friend class StateIterator<RelabelFst<A>>;

  RelabelFst(const Fst<Arc> &fst,
             const std::vector<std::pair<Label, Label>> &ipairs,
             const std::vector<std::pair<Label, Label>> &opairs,
             const RelabelFstOptions &opts = RelabelFstOptions())
      : Base(std::make_shared<Impl>(fst, ipairs, opairs, opts)) {}

  RelabelFst(const Fst<Arc> &fst, const SymbolTable *new_isymbols,
             const SymbolTable *new_osymbols,
             const RelabelFstOptions &opts = RelabelFstOptions())
      : Base(std::make_shared<Impl>(fst, fst.InputSymbols(), new_isymbols,
                                    fst.OutputSymbols(), new_osymbols, opts)) {}

  RelabelFst(const Fst<Arc> &fst, const SymbolTable *old_isymbols,
             const SymbolTable *new_isymbols, const SymbolTable *old_osymbols,
             const SymbolTable *new_osymbols,
             const RelabelFstOptions &opts = RelabelFstOptions())
      : Base(std::make_shared<Impl>(fst, old_isymbols, new_isymbols,
                                    old_osymbols, new_osymbols, opts)) {}

  // See Fst<>::Copy() for doc.
  RelabelFst(const RelabelFst &fst, bool safe = false) : Base(fst, safe) {}

  // Gets a copy of this RelabelFst. See Fst<>::Copy() for further doc.
  RelabelFst *Copy(bool safe = false) const override {
    return new RelabelFst(*this, safe);
  }

  void InitStateIterator(StateIteratorData<Arc> *data) const override;

  void InitArcIterator(StateId s, ArcIteratorData<Arc> *data) const override {
    return GetMutableImpl()->InitArcIterator(s, data);
  }

 private:
  using Base::GetImpl;
  using Base::GetMutableImpl;

  RelabelFst &operator=(const RelabelFst &) = delete;
};

// Specialization for RelabelFst.
template <class Arc>
class StateIterator<RelabelFst<Arc>> : public StateIteratorBase<Arc> {
 public:
  using StateId = typename Arc::StateId;

  explicit StateIterator(const RelabelFst<Arc> &fst)
      : impl_(fst.GetImpl()), siter_(*impl_->fst_), s_(0) {}

  bool Done() const final { return siter_.Done(); }

  StateId Value() const final { return s_; }

  void Next() final {
    if (!siter_.Done()) {
      ++s_;
      siter_.Next();
    }
  }

  void Reset() final {
    s_ = 0;
    siter_.Reset();
  }

 private:
  const internal::RelabelFstImpl<Arc> *impl_;
  StateIterator<Fst<Arc>> siter_;
  StateId s_;

  StateIterator(const StateIterator &) = delete;
  StateIterator &operator=(const StateIterator &) = delete;
};

// Specialization for RelabelFst.
template <class Arc>
class ArcIterator<RelabelFst<Arc>> : public CacheArcIterator<RelabelFst<Arc>> {
 public:
  using StateId = typename Arc::StateId;

  ArcIterator(const RelabelFst<Arc> &fst, StateId s)
      : CacheArcIterator<RelabelFst<Arc>>(fst.GetMutableImpl(), s) {
    if (!fst.GetImpl()->HasArcs(s)) fst.GetMutableImpl()->Expand(s);
  }
};

template <class Arc>
inline void RelabelFst<Arc>::InitStateIterator(
    StateIteratorData<Arc> *data) const {
  data->base = std::make_unique<StateIterator<RelabelFst<Arc>>>(*this);
}

// Useful alias when using StdArc.
using StdRelabelFst = RelabelFst<StdArc>;

}  // namespace fst

#endif  // FST_RELABEL_H_
