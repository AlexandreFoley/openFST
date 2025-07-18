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
// Regression test for various FST algorithms.

#ifndef FST_TEST_ALGO_TEST_H_
#define FST_TEST_ALGO_TEST_H_

#include <sys/types.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include <fst/log.h>
#include <fst/arc-map.h>
#include <fst/arc.h>
#include <fst/arcfilter.h>
#include <fst/arcsort.h>
#include <fst/cache.h>
#include <fst/closure.h>
#include <fst/compose-filter.h>
#include <fst/compose.h>
#include <fst/concat.h>
#include <fst/connect.h>
#include <fst/determinize.h>
#include <fst/dfs-visit.h>
#include <fst/difference.h>
#include <fst/disambiguate.h>
#include <fst/encode.h>
#include <fst/equivalent.h>
#include <fst/float-weight.h>
#include <fst/fst.h>
#include <fst/fstlib.h>
#include <fst/intersect.h>
#include <fst/invert.h>
#include <fst/lookahead-matcher.h>
#include <fst/matcher-fst.h>
#include <fst/matcher.h>
#include <fst/minimize.h>
#include <fst/mutable-fst.h>
#include <fst/pair-weight.h>
#include <fst/project.h>
#include <fst/properties.h>
#include <fst/prune.h>
#include <fst/push.h>
#include <fst/randequivalent.h>
#include <fst/randgen.h>
#include <fst/rational.h>
#include <fst/relabel.h>
#include <fst/reverse.h>
#include <fst/reweight.h>
#include <fst/rmepsilon.h>
#include <fst/shortest-distance.h>
#include <fst/shortest-path.h>
#include <fst/string-weight.h>
#include <fst/synchronize.h>
#include <fst/topsort.h>
#include <fst/union-weight.h>
#include <fst/union.h>
#include <fst/vector-fst.h>
#include <fst/verify.h>
#include <fst/weight.h>
#include <fst/test/rand-fst.h>

DECLARE_int32(repeat);  // defined in ./algo_test.cc

namespace fst {

// Mapper to change input and output label of every transition into
// epsilons.
template <class A>
class EpsMapper {
 public:
  EpsMapper() = default;

  A operator()(const A &arc) const {
    return A(0, 0, arc.weight, arc.nextstate);
  }

  uint64_t Properties(uint64_t props) const {
    props &= ~kNotAcceptor;
    props |= kAcceptor;
    props &= ~kNoIEpsilons & ~kNoOEpsilons & ~kNoEpsilons;
    props |= kIEpsilons | kOEpsilons | kEpsilons;
    props &= ~kNotILabelSorted & ~kNotOLabelSorted;
    props |= kILabelSorted | kOLabelSorted;
    return props;
  }

  MapFinalAction FinalAction() const { return MAP_NO_SUPERFINAL; }

  MapSymbolsAction InputSymbolsAction() const { return MAP_COPY_SYMBOLS; }

  MapSymbolsAction OutputSymbolsAction() const { return MAP_COPY_SYMBOLS; }
};

// Generic - no lookahead.
template <class Arc>
void LookAheadCompose(const Fst<Arc> &ifst1, const Fst<Arc> &ifst2,
                      MutableFst<Arc> *ofst) {
  Compose(ifst1, ifst2, ofst);
}

// Specialized and epsilon olabel acyclic - lookahead.
inline void LookAheadCompose(const Fst<StdArc> &ifst1, const Fst<StdArc> &ifst2,
                             MutableFst<StdArc> *ofst) {
  std::vector<StdArc::StateId> order;
  bool acyclic;
  TopOrderVisitor<StdArc> visitor(&order, &acyclic);
  DfsVisit(ifst1, &visitor, OutputEpsilonArcFilter<StdArc>());
  if (acyclic) {  // no ifst1 output epsilon cycles?
    StdOLabelLookAheadFst lfst1(ifst1);
    StdVectorFst lfst2(ifst2);
    LabelLookAheadRelabeler<StdArc>::Relabel(&lfst2, lfst1, true);
    Compose(lfst1, lfst2, ofst);
  } else {
    Compose(ifst1, ifst2, ofst);
  }
}

// This class tests a variety of identities and properties that must
// hold for various algorithms on weighted FSTs.
template <class Arc>
class WeightedTester {
 public:
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;
  using WeightGenerator = WeightGenerate<Weight>;

  WeightedTester(uint64_t seed, const Fst<Arc> &zero_fst,
                 const Fst<Arc> &one_fst, const Fst<Arc> &univ_fst,
                 WeightGenerator weight_generator)
      : seed_(seed),
        rand_(seed),
        zero_fst_(zero_fst),
        one_fst_(one_fst),
        univ_fst_(univ_fst),
        generate_(std::move(weight_generator)) {}

  void Test(const Fst<Arc> &T1, const Fst<Arc> &T2, const Fst<Arc> &T3) {
    TestRational(T1, T2, T3);
    TestMap(T1);
    TestCompose(T1, T2, T3);
    TestSort(T1);
    TestOptimize(T1);
    TestSearch(T1);
  }

 private:
  // Tests rational operations with identities
  void TestRational(const Fst<Arc> &T1, const Fst<Arc> &T2,
                    const Fst<Arc> &T3) {
    {
      VLOG(1) << "Check destructive and delayed union are equivalent.";
      VectorFst<Arc> U1(T1);
      Union(&U1, T2);
      UnionFst<Arc> U2(T1, T2);
      FST_CHECK(Equiv(U1, U2));
    }

    {
      VLOG(1) << "Check destructive and delayed concatenation are equivalent.";
      VectorFst<Arc> C1(T1);
      Concat(&C1, T2);
      ConcatFst<Arc> C2(T1, T2);
      FST_CHECK(Equiv(C1, C2));
      VectorFst<Arc> C3(T2);
      Concat(T1, &C3);
      FST_CHECK(Equiv(C3, C2));
    }

    {
      VLOG(1) << "Check destructive and delayed closure* are equivalent.";
      VectorFst<Arc> C1(T1);
      Closure(&C1, CLOSURE_STAR);
      ClosureFst<Arc> C2(T1, CLOSURE_STAR);
      FST_CHECK(Equiv(C1, C2));
    }

    {
      VLOG(1) << "Check destructive and delayed closure+ are equivalent.";
      VectorFst<Arc> C1(T1);
      Closure(&C1, CLOSURE_PLUS);
      ClosureFst<Arc> C2(T1, CLOSURE_PLUS);
      FST_CHECK(Equiv(C1, C2));
    }

    {
      VLOG(1) << "Check union is associative (destructive).";
      VectorFst<Arc> U1(T1);
      Union(&U1, T2);
      Union(&U1, T3);

      VectorFst<Arc> U3(T2);
      Union(&U3, T3);
      VectorFst<Arc> U4(T1);
      Union(&U4, U3);

      FST_CHECK(Equiv(U1, U4));
    }

    {
      VLOG(1) << "Check union is associative (delayed).";
      UnionFst<Arc> U1(T1, T2);
      UnionFst<Arc> U2(U1, T3);

      UnionFst<Arc> U3(T2, T3);
      UnionFst<Arc> U4(T1, U3);

      FST_CHECK(Equiv(U2, U4));
    }

    {
      VLOG(1) << "Check union is associative (destructive delayed).";
      UnionFst<Arc> U1(T1, T2);
      Union(&U1, T3);

      UnionFst<Arc> U3(T2, T3);
      UnionFst<Arc> U4(T1, U3);

      FST_CHECK(Equiv(U1, U4));
    }

    {
      VLOG(1) << "Check concatenation is associative (destructive).";
      VectorFst<Arc> C1(T1);
      Concat(&C1, T2);
      Concat(&C1, T3);

      VectorFst<Arc> C3(T2);
      Concat(&C3, T3);
      VectorFst<Arc> C4(T1);
      Concat(&C4, C3);

      FST_CHECK(Equiv(C1, C4));
    }

    {
      VLOG(1) << "Check concatenation is associative (delayed).";
      ConcatFst<Arc> C1(T1, T2);
      ConcatFst<Arc> C2(C1, T3);

      ConcatFst<Arc> C3(T2, T3);
      ConcatFst<Arc> C4(T1, C3);

      FST_CHECK(Equiv(C2, C4));
    }

    {
      VLOG(1) << "Check concatenation is associative (destructive delayed).";
      ConcatFst<Arc> C1(T1, T2);
      Concat(&C1, T3);

      ConcatFst<Arc> C3(T2, T3);
      ConcatFst<Arc> C4(T1, C3);

      FST_CHECK(Equiv(C1, C4));
    }

    if (Weight::Properties() & kLeftSemiring) {
      VLOG(1) << "Check concatenation left distributes"
              << " over union (destructive).";

      VectorFst<Arc> U1(T1);
      Union(&U1, T2);
      VectorFst<Arc> C1(T3);
      Concat(&C1, U1);

      VectorFst<Arc> C2(T3);
      Concat(&C2, T1);
      VectorFst<Arc> C3(T3);
      Concat(&C3, T2);
      VectorFst<Arc> U2(C2);
      Union(&U2, C3);

      FST_CHECK(Equiv(C1, U2));
    }

    if (Weight::Properties() & kRightSemiring) {
      VLOG(1) << "Check concatenation right distributes"
              << " over union (destructive).";
      VectorFst<Arc> U1(T1);
      Union(&U1, T2);
      VectorFst<Arc> C1(U1);
      Concat(&C1, T3);

      VectorFst<Arc> C2(T1);
      Concat(&C2, T3);
      VectorFst<Arc> C3(T2);
      Concat(&C3, T3);
      VectorFst<Arc> U2(C2);
      Union(&U2, C3);

      FST_CHECK(Equiv(C1, U2));
    }

    if (Weight::Properties() & kLeftSemiring) {
      VLOG(1) << "Check concatenation left distributes over union (delayed).";
      UnionFst<Arc> U1(T1, T2);
      ConcatFst<Arc> C1(T3, U1);

      ConcatFst<Arc> C2(T3, T1);
      ConcatFst<Arc> C3(T3, T2);
      UnionFst<Arc> U2(C2, C3);

      FST_CHECK(Equiv(C1, U2));
    }

    if (Weight::Properties() & kRightSemiring) {
      VLOG(1) << "Check concatenation right distributes over union (delayed).";
      UnionFst<Arc> U1(T1, T2);
      ConcatFst<Arc> C1(U1, T3);

      ConcatFst<Arc> C2(T1, T3);
      ConcatFst<Arc> C3(T2, T3);
      UnionFst<Arc> U2(C2, C3);

      FST_CHECK(Equiv(C1, U2));
    }

    if (Weight::Properties() & kLeftSemiring) {
      VLOG(1) << "Check T T* == T+ (destructive).";
      VectorFst<Arc> S(T1);
      Closure(&S, CLOSURE_STAR);
      VectorFst<Arc> C(T1);
      Concat(&C, S);

      VectorFst<Arc> P(T1);
      Closure(&P, CLOSURE_PLUS);

      FST_CHECK(Equiv(C, P));
    }

    if (Weight::Properties() & kRightSemiring) {
      VLOG(1) << "Check T* T == T+ (destructive).";
      VectorFst<Arc> S(T1);
      Closure(&S, CLOSURE_STAR);
      VectorFst<Arc> C(S);
      Concat(&C, T1);

      VectorFst<Arc> P(T1);
      Closure(&P, CLOSURE_PLUS);

      FST_CHECK(Equiv(C, P));
    }

    if (Weight::Properties() & kLeftSemiring) {
      VLOG(1) << "Check T T* == T+ (delayed).";
      ClosureFst<Arc> S(T1, CLOSURE_STAR);
      ConcatFst<Arc> C(T1, S);

      ClosureFst<Arc> P(T1, CLOSURE_PLUS);

      FST_CHECK(Equiv(C, P));
    }

    if (Weight::Properties() & kRightSemiring) {
      VLOG(1) << "Check T* T == T+ (delayed).";
      ClosureFst<Arc> S(T1, CLOSURE_STAR);
      ConcatFst<Arc> C(S, T1);

      ClosureFst<Arc> P(T1, CLOSURE_PLUS);

      FST_CHECK(Equiv(C, P));
    }
  }

  // Tests map-based operations.
  void TestMap(const Fst<Arc> &T) {
    {
      VLOG(1) << "Check destructive and delayed projection are equivalent.";
      VectorFst<Arc> P1(T);
      Project(&P1, ProjectType::INPUT);
      ProjectFst<Arc> P2(T, ProjectType::INPUT);
      FST_CHECK(Equiv(P1, P2));
    }

    {
      VLOG(1) << "Check destructive and delayed inversion are equivalent.";
      VectorFst<Arc> I1(T);
      Invert(&I1);
      InvertFst<Arc> I2(T);
      FST_CHECK(Equiv(I1, I2));
    }

    {
      VLOG(1) << "Check Pi_1(T) = Pi_2(T^-1) (destructive).";
      VectorFst<Arc> P1(T);
      VectorFst<Arc> I1(T);
      Project(&P1, ProjectType::INPUT);
      Invert(&I1);
      Project(&I1, ProjectType::OUTPUT);
      FST_CHECK(Equiv(P1, I1));
    }

    {
      VLOG(1) << "Check Pi_2(T) = Pi_1(T^-1) (destructive).";
      VectorFst<Arc> P1(T);
      VectorFst<Arc> I1(T);
      Project(&P1, ProjectType::OUTPUT);
      Invert(&I1);
      Project(&I1, ProjectType::INPUT);
      FST_CHECK(Equiv(P1, I1));
    }

    {
      VLOG(1) << "Check Pi_1(T) = Pi_2(T^-1) (delayed).";
      ProjectFst<Arc> P1(T, ProjectType::INPUT);
      InvertFst<Arc> I1(T);
      ProjectFst<Arc> P2(I1, ProjectType::OUTPUT);
      FST_CHECK(Equiv(P1, P2));
    }

    {
      VLOG(1) << "Check Pi_2(T) = Pi_1(T^-1) (delayed).";
      ProjectFst<Arc> P1(T, ProjectType::OUTPUT);
      InvertFst<Arc> I1(T);
      ProjectFst<Arc> P2(I1, ProjectType::INPUT);
      FST_CHECK(Equiv(P1, P2));
    }

    {
      VLOG(1) << "Check destructive relabeling";
      static const int kNumLabels = 10;
      // set up relabeling pairs
      std::vector<Label> labelset(kNumLabels);
      for (size_t i = 0; i < kNumLabels; ++i) labelset[i] = i;
      for (size_t i = 0; i < kNumLabels; ++i) {
        using std::swap;
        const auto index =
            std::uniform_int_distribution<>(0, kNumLabels - 1)(rand_);
        swap(labelset[i], labelset[index]);
      }

      std::vector<std::pair<Label, Label>> ipairs1(kNumLabels);
      std::vector<std::pair<Label, Label>> opairs1(kNumLabels);
      for (size_t i = 0; i < kNumLabels; ++i) {
        ipairs1[i] = std::make_pair(i, labelset[i]);
        opairs1[i] = std::make_pair(labelset[i], i);
      }
      VectorFst<Arc> R(T);
      Relabel(&R, ipairs1, opairs1);

      std::vector<std::pair<Label, Label>> ipairs2(kNumLabels);
      std::vector<std::pair<Label, Label>> opairs2(kNumLabels);
      for (size_t i = 0; i < kNumLabels; ++i) {
        ipairs2[i] = std::make_pair(labelset[i], i);
        opairs2[i] = std::make_pair(i, labelset[i]);
      }
      Relabel(&R, ipairs2, opairs2);
      FST_CHECK(Equiv(R, T));

      VLOG(1) << "Check on-the-fly relabeling";
      RelabelFst<Arc> Rdelay(T, ipairs1, opairs1);

      RelabelFst<Arc> RRdelay(Rdelay, ipairs2, opairs2);
      FST_CHECK(Equiv(RRdelay, T));
    }

    {
      VLOG(1) << "Check encoding/decoding (destructive).";
      VectorFst<Arc> D(T);
      uint8_t encode_props = 0;
      if (std::bernoulli_distribution(.5)(rand_)) {
        encode_props |= kEncodeLabels;
      }
      if (std::bernoulli_distribution(.5)(rand_)) {
        encode_props |= kEncodeWeights;
      }
      EncodeMapper<Arc> encoder(encode_props, ENCODE);
      Encode(&D, &encoder);
      Decode(&D, encoder);
      FST_CHECK(Equiv(D, T));
    }

    {
      VLOG(1) << "Check encoding/decoding (delayed).";
      uint8_t encode_props = 0;
      if (std::bernoulli_distribution(.5)(rand_)) {
        encode_props |= kEncodeLabels;
      }
      if (std::bernoulli_distribution(.5)(rand_)) {
        encode_props |= kEncodeWeights;
      }
      EncodeMapper<Arc> encoder(encode_props, ENCODE);
      EncodeFst<Arc> E(T, &encoder);
      VectorFst<Arc> Encoded(E);
      DecodeFst<Arc> D(Encoded, encoder);
      FST_CHECK(Equiv(D, T));
    }

    {
      VLOG(1) << "Check gallic mappers (constructive).";
      ToGallicMapper<Arc> to_mapper;
      FromGallicMapper<Arc> from_mapper;
      VectorFst<GallicArc<Arc>> G;
      VectorFst<Arc> F;
      ArcMap(T, &G, to_mapper);
      ArcMap(G, &F, from_mapper);
      FST_CHECK(Equiv(T, F));
    }

    {
      VLOG(1) << "Check gallic mappers (delayed).";
      ArcMapFst G(T, ToGallicMapper<Arc>());
      ArcMapFst F(G, FromGallicMapper<Arc>());
      FST_CHECK(Equiv(T, F));
    }
  }

  // Tests compose-based operations.
  void TestCompose(const Fst<Arc> &T1, const Fst<Arc> &T2, const Fst<Arc> &T3) {
    if (!(Weight::Properties() & kCommutative)) return;

    VectorFst<Arc> S1(T1);
    VectorFst<Arc> S2(T2);
    VectorFst<Arc> S3(T3);

    ILabelCompare<Arc> icomp;
    OLabelCompare<Arc> ocomp;

    ArcSort(&S1, ocomp);
    ArcSort(&S2, ocomp);
    ArcSort(&S3, icomp);

    {
      VLOG(1) << "Check composition is associative.";
      ComposeFst<Arc> C1(S1, S2);
      ComposeFst<Arc> C2(C1, S3);
      ComposeFst<Arc> C3(S2, S3);
      ComposeFst<Arc> C4(S1, C3);

      FST_CHECK(Equiv(C2, C4));
    }

    {
      VLOG(1) << "Check composition left distributes over union.";
      UnionFst<Arc> U1(S2, S3);
      ComposeFst<Arc> C1(S1, U1);

      ComposeFst<Arc> C2(S1, S2);
      ComposeFst<Arc> C3(S1, S3);
      UnionFst<Arc> U2(C2, C3);

      FST_CHECK(Equiv(C1, U2));
    }

    {
      VLOG(1) << "Check composition right distributes over union.";
      UnionFst<Arc> U1(S1, S2);
      ComposeFst<Arc> C1(U1, S3);

      ComposeFst<Arc> C2(S1, S3);
      ComposeFst<Arc> C3(S2, S3);
      UnionFst<Arc> U2(C2, C3);

      FST_CHECK(Equiv(C1, U2));
    }

    VectorFst<Arc> A1(S1);
    VectorFst<Arc> A2(S2);
    VectorFst<Arc> A3(S3);
    Project(&A1, ProjectType::OUTPUT);
    Project(&A2, ProjectType::INPUT);
    Project(&A3, ProjectType::INPUT);

    {
      VLOG(1) << "Check intersection is commutative.";
      IntersectFst<Arc> I1(A1, A2);
      IntersectFst<Arc> I2(A2, A1);
      FST_CHECK(Equiv(I1, I2));
    }

    {
      VLOG(1) << "Check all epsilon filters leads to equivalent results.";
      using M = Matcher<Fst<Arc>>;
      ComposeFst<Arc> C1(S1, S2);
      ComposeFst<Arc> C2(
          S1, S2, ComposeFstOptions<Arc, M, AltSequenceComposeFilter<M>>());
      ComposeFst<Arc> C3(S1, S2,
                         ComposeFstOptions<Arc, M, MatchComposeFilter<M>>());

      FST_CHECK(Equiv(C1, C2));
      FST_CHECK(Equiv(C1, C3));

      if ((Weight::Properties() & kIdempotent) ||
          S1.Properties(kNoOEpsilons, false) ||
          S2.Properties(kNoIEpsilons, false)) {
        ComposeFst<Arc> C4(
            S1, S2, ComposeFstOptions<Arc, M, TrivialComposeFilter<M>>());
        FST_CHECK(Equiv(C1, C4));
        ComposeFst<Arc> C5(
            S1, S2, ComposeFstOptions<Arc, M, NoMatchComposeFilter<M>>());
        FST_CHECK(Equiv(C1, C5));
      }

      if (S1.Properties(kNoOEpsilons, false) &&
          S2.Properties(kNoIEpsilons, false)) {
        ComposeFst<Arc> C6(S1, S2,
                           ComposeFstOptions<Arc, M, NullComposeFilter<M>>());
        FST_CHECK(Equiv(C1, C6));
      }
    }

    {
      VLOG(1) << "Check look-ahead filters lead to equivalent results.";
      VectorFst<Arc> C1, C2;
      Compose(S1, S2, &C1);
      LookAheadCompose(S1, S2, &C2);
      FST_CHECK(Equiv(C1, C2));
    }
  }

  // Tests sorting operations
  void TestSort(const Fst<Arc> &T) {
    ILabelCompare<Arc> icomp;
    OLabelCompare<Arc> ocomp;

    {
      VLOG(1) << "Check arc sorted Fst is equivalent to its input.";
      VectorFst<Arc> S1(T);
      ArcSort(&S1, icomp);
      FST_CHECK(Equiv(T, S1));
    }

    {
      VLOG(1) << "Check destructive and delayed arcsort are equivalent.";
      VectorFst<Arc> S1(T);
      ArcSort(&S1, icomp);
      ArcSortFst<Arc, ILabelCompare<Arc>> S2(T, icomp);
      FST_CHECK(Equiv(S1, S2));
    }

    {
      VLOG(1) << "Check ilabel sorting vs. olabel sorting with inversions.";
      VectorFst<Arc> S1(T);
      VectorFst<Arc> S2(T);
      ArcSort(&S1, icomp);
      Invert(&S2);
      ArcSort(&S2, ocomp);
      Invert(&S2);
      FST_CHECK(Equiv(S1, S2));
    }

    {
      VLOG(1) << "Check topologically sorted Fst is equivalent to its input.";
      VectorFst<Arc> S1(T);
      TopSort(&S1);
      FST_CHECK(Equiv(T, S1));
    }

    {
      VLOG(1) << "Check reverse(reverse(T)) = T";
      for (int i = 0; i < 2; ++i) {
        VectorFst<ReverseArc<Arc>> R1;
        VectorFst<Arc> R2;
        bool require_superinitial = i == 1;
        Reverse(T, &R1, require_superinitial);
        Reverse(R1, &R2, require_superinitial);
        FST_CHECK(Equiv(T, R2));
      }
    }
  }

  // Tests optimization operations
  void TestOptimize(const Fst<Arc> &T) {
    uint64_t tprops = T.Properties(kFstProperties, true);
    uint64_t wprops = Weight::Properties();

    VectorFst<Arc> A(T);
    Project(&A, ProjectType::INPUT);
    {
      VLOG(1) << "Check connected FST is equivalent to its input.";
      VectorFst<Arc> C1(T);
      Connect(&C1);
      FST_CHECK(Equiv(T, C1));
    }

    if ((wprops & kSemiring) == kSemiring &&
        (tprops & kAcyclic || wprops & kIdempotent)) {
      VLOG(1) << "Check epsilon-removed FST is equivalent to its input.";
      VectorFst<Arc> R1(T);
      RmEpsilon(&R1);
      FST_CHECK(Equiv(T, R1));

      VLOG(1) << "Check destructive and delayed epsilon removal"
              << "are equivalent.";
      RmEpsilonFst<Arc> R2(T);
      FST_CHECK(Equiv(R1, R2));

      VLOG(1) << "Check an FST with a large proportion"
              << " of epsilon transitions:";
      // Maps all transitions of T to epsilon-transitions and append
      // a non-epsilon transition.
      VectorFst<Arc> U;
      ArcMap(T, &U, EpsMapper<Arc>());
      VectorFst<Arc> V;
      V.SetStart(V.AddState());
      Arc arc(1, 1, Weight::One(), V.AddState());
      V.AddArc(V.Start(), arc);
      V.SetFinal(arc.nextstate, Weight::One());
      Concat(&U, V);
      // Check that epsilon-removal preserves the shortest-distance
      // from the initial state to the final states.
      std::vector<Weight> d;
      ShortestDistance(U, &d, true);
      Weight w = U.Start() < d.size() ? d[U.Start()] : Weight::Zero();
      VectorFst<Arc> U1(U);
      RmEpsilon(&U1);
      ShortestDistance(U1, &d, true);
      Weight w1 = U1.Start() < d.size() ? d[U1.Start()] : Weight::Zero();
      FST_CHECK(ApproxEqual(w, w1, kTestDelta));
      RmEpsilonFst<Arc> U2(U);
      ShortestDistance(U2, &d, true);
      Weight w2 = U2.Start() < d.size() ? d[U2.Start()] : Weight::Zero();
      FST_CHECK(ApproxEqual(w, w2, kTestDelta));
    }

    if ((wprops & kSemiring) == kSemiring && tprops & kAcyclic) {
      VLOG(1) << "Check determinized FSA is equivalent to its input.";
      DeterminizeFst<Arc> D(A);
      FST_CHECK(Equiv(A, D));

      {
        VLOG(1) << "Check determinized FST is equivalent to its input.";
        DeterminizeFstOptions<Arc> opts;
        opts.type = DETERMINIZE_NONFUNCTIONAL;
        DeterminizeFst<Arc> DT(T, opts);
        FST_CHECK(Equiv(T, DT));
      }

      if ((wprops & (kPath | kCommutative)) == (kPath | kCommutative)) {
        VLOG(1) << "Check pruning in determinization";
        VectorFst<Arc> P;
        const Weight threshold = generate_();
        DeterminizeOptions<Arc> opts;
        opts.weight_threshold = threshold;
        Determinize(A, &P, opts);
        FST_CHECK(P.Properties(kIDeterministic, true));
        FST_CHECK(PruneEquiv(A, P, threshold));
      }

      if ((wprops & kPath) == kPath) {
        VLOG(1) << "Check min-determinization";

        // Ensures no input epsilons
        VectorFst<Arc> R(T);
        std::vector<std::pair<Label, Label>> ipairs, opairs;
        ipairs.push_back(std::pair<Label, Label>(0, 1));
        Relabel(&R, ipairs, opairs);

        VectorFst<Arc> M;
        DeterminizeOptions<Arc> opts;
        opts.type = DETERMINIZE_DISAMBIGUATE;
        Determinize(R, &M, opts);
        FST_CHECK(M.Properties(kIDeterministic, true));
        FST_CHECK(MinRelated(M, R));
      }

      int n;
      {
        VLOG(1) << "Check size(min(det(A))) <= size(det(A))"
                << " and  min(det(A)) equiv det(A)";
        VectorFst<Arc> M(D);
        n = M.NumStates();
        Minimize(&M, static_cast<MutableFst<Arc> *>(nullptr), kDelta);
        FST_CHECK(Equiv(D, M));
        FST_CHECK(M.NumStates() <= n);
        n = M.NumStates();
      }

      if (n && (wprops & kIdempotent) == kIdempotent &&
          A.Properties(kNoEpsilons, true)) {
        VLOG(1) << "Check that Revuz's algorithm leads to the"
                << " same number of states as Brozozowski's algorithm";

        // Skip test if A is the empty machine or contains epsilons or
        // if the semiring is not idempotent (to avoid floating point
        // errors)
        VectorFst<ReverseArc<Arc>> R;
        Reverse(A, &R);
        RmEpsilon(&R);
        DeterminizeFst<ReverseArc<Arc>> DR(R);
        VectorFst<Arc> RD;
        Reverse(DR, &RD);
        DeterminizeFst<Arc> DRD(RD);
        VectorFst<Arc> M(DRD);
        FST_CHECK_EQ(n + 1, M.NumStates());  // Accounts for the epsilon transition
                                         // to the initial state
      }
    }

    if ((wprops & kSemiring) == kSemiring && tprops & kAcyclic) {
      VLOG(1) << "Check disambiguated FSA is equivalent to its input.";
      VectorFst<Arc> R(A), D;
      RmEpsilon(&R);
      Disambiguate(R, &D);
      FST_CHECK(Equiv(R, D));
      VLOG(1) << "Check disambiguated FSA is unambiguous";
      FST_CHECK(Unambiguous(D));

      /* TODO(riley): find out why this fails
      if ((wprops & (kPath | kCommutative)) == (kPath | kCommutative)) {
        VLOG(1)  << "Check pruning in disambiguation";
        VectorFst<Arc> P;
        const Weight threshold = generate_();
        DisambiguateOptions<Arc> opts;
        opts.weight_threshold = threshold;
        Disambiguate(R, &P, opts);
        FST_CHECK(Unambiguous(P));
        FST_CHECK(PruneEquiv(A, P, threshold));
      }
      */
    }

    if (Arc::Type() == LogArc::Type() || Arc::Type() == StdArc::Type()) {
      VLOG(1) << "Check reweight(T) equiv T";
      std::vector<Weight> potential;
      VectorFst<Arc> RI(T);
      VectorFst<Arc> RF(T);
      while (potential.size() < RI.NumStates()) {
        potential.push_back(generate_());
      }

      Reweight(&RI, potential, REWEIGHT_TO_INITIAL);
      FST_CHECK(Equiv(T, RI));

      Reweight(&RF, potential, REWEIGHT_TO_FINAL);
      FST_CHECK(Equiv(T, RF));
    }

    if ((wprops & kIdempotent) || (tprops & kAcyclic)) {
      VLOG(1) << "Check pushed FST is equivalent to input FST.";
      // Pushing towards the final state.
      if (wprops & kRightSemiring) {
        VectorFst<Arc> P1;
        Push<Arc, REWEIGHT_TO_FINAL>(T, &P1, kPushLabels);
        FST_CHECK(Equiv(T, P1));

        VectorFst<Arc> P2;
        Push<Arc, REWEIGHT_TO_FINAL>(T, &P2, kPushWeights);
        FST_CHECK(Equiv(T, P2));

        VectorFst<Arc> P3;
        Push<Arc, REWEIGHT_TO_FINAL>(T, &P3, kPushLabels | kPushWeights);
        FST_CHECK(Equiv(T, P3));
      }

      // Pushing towards the initial state.
      if (wprops & kLeftSemiring) {
        VectorFst<Arc> P1;
        Push<Arc, REWEIGHT_TO_INITIAL>(T, &P1, kPushLabels);
        FST_CHECK(Equiv(T, P1));

        VectorFst<Arc> P2;
        Push<Arc, REWEIGHT_TO_INITIAL>(T, &P2, kPushWeights);
        FST_CHECK(Equiv(T, P2));
        VectorFst<Arc> P3;
        Push<Arc, REWEIGHT_TO_INITIAL>(T, &P3, kPushLabels | kPushWeights);
        FST_CHECK(Equiv(T, P3));
      }
    }

    if constexpr (IsPath<Weight>::value) {
      if ((wprops & (kPath | kCommutative)) == (kPath | kCommutative)) {
        VLOG(1) << "Check pruning algorithm";
        {
          VLOG(1) << "Check equiv. of constructive and destructive algorithms";
          const Weight threshold = generate_();
          VectorFst<Arc> P1(T);
          Prune(&P1, threshold);
          VectorFst<Arc> P2;
          Prune(T, &P2, threshold);
          FST_CHECK(Equiv(P1, P2));
        }

        {
          VLOG(1) << "Check prune(reverse) equiv reverse(prune)";
          const Weight threshold = generate_();
          VectorFst<ReverseArc<Arc>> R;
          VectorFst<Arc> P1(T);
          VectorFst<Arc> P2;
          Prune(&P1, threshold);
          Reverse(T, &R);
          Prune(&R, threshold.Reverse());
          Reverse(R, &P2);
          FST_CHECK(Equiv(P1, P2));
        }
        {
          VLOG(1) << "Check: ShortestDistance(A - prune(A))"
                  << " > ShortestDistance(A) times Threshold";
          const Weight threshold = generate_();
          VectorFst<Arc> P;
          Prune(A, &P, threshold);
          FST_CHECK(PruneEquiv(A, P, threshold));
        }
      }
    }
    if (tprops & kAcyclic) {
      VLOG(1) << "Check synchronize(T) equiv T";
      SynchronizeFst<Arc> S(T);
      FST_CHECK(Equiv(T, S));
    }
  }

  // Tests search operations
  void TestSearch(const Fst<Arc> &T) {
    if constexpr (IsPath<Weight>::value) {
      uint64_t wprops = Weight::Properties();

      VectorFst<Arc> A(T);
      Project(&A, ProjectType::INPUT);

      if ((wprops & (kPath | kRightSemiring)) == (kPath | kRightSemiring)) {
        VLOG(1) << "Check 1-best weight.";
        VectorFst<Arc> path;
        ShortestPath(T, &path);
        Weight tsum = ShortestDistance(T);
        Weight psum = ShortestDistance(path);
        FST_CHECK(ApproxEqual(tsum, psum, kTestDelta));
      }

      if ((wprops & (kPath | kSemiring)) == (kPath | kSemiring)) {
        VLOG(1) << "Check n-best weights";
        VectorFst<Arc> R(A);
        RmEpsilon(&R, /*connect=*/true, Arc::Weight::Zero(), kNoStateId,
                  kDelta);
        const int nshortest = std::uniform_int_distribution<>(
            0, kNumRandomShortestPaths + 1)(rand_);
        VectorFst<Arc> paths;
        ShortestPath(R, &paths, nshortest, /*unique=*/true,
                     /*first_path=*/false, Weight::Zero(), kNumShortestStates,
                     kDelta);
        std::vector<Weight> distance;
        ShortestDistance(paths, &distance, true, kDelta);
        StateId pstart = paths.Start();
        if (pstart != kNoStateId) {
          ArcIterator<Fst<Arc>> piter(paths, pstart);
          for (; !piter.Done(); piter.Next()) {
            StateId s = piter.Value().nextstate;
            Weight nsum = s < distance.size()
                              ? Times(piter.Value().weight, distance[s])
                              : Weight::Zero();
            VectorFst<Arc> path;
            ShortestPath(R, &path, 1, false, false, Weight::Zero(), kNoStateId,
                         kDelta);
            Weight dsum = ShortestDistance(path, kDelta);
            FST_CHECK(ApproxEqual(nsum, dsum, kTestDelta));
            ArcMap(&path, RmWeightMapper<Arc>());
            VectorFst<Arc> S;
            Difference(R, path, &S);
            R = S;
          }
        }
      }
    }
  }

  // Tests if two FSTS are equivalent by checking if random
  // strings from one FST are transduced the same by both FSTs.
  template <class A>
  bool Equiv(const Fst<A> &fst1, const Fst<A> &fst2) {
    VLOG(1) << "Check FSTs for sanity (including property bits).";
    FST_CHECK(Verify(fst1));
    FST_CHECK(Verify(fst2));

    // Ensures seed used once per instantiation.
    static const UniformArcSelector<A> uniform_selector(seed_);
    const RandGenOptions<UniformArcSelector<A>> opts(uniform_selector,
                                                     kRandomPathLength);
    return RandEquivalent(fst1, fst2, kNumRandomPaths, opts, kTestDelta, seed_);
  }

  // Tests FSA is unambiguous.
  bool Unambiguous(const Fst<Arc> &fst) {
    VectorFst<StdArc> sfst, dfst;
    VectorFst<LogArc> lfst1, lfst2;
    ArcMap(fst, &sfst, RmWeightMapper<Arc, StdArc>());
    Determinize(sfst, &dfst);
    ArcMap(fst, &lfst1, RmWeightMapper<Arc, LogArc>());
    ArcMap(dfst, &lfst2, RmWeightMapper<StdArc, LogArc>());
    return Equiv(lfst1, lfst2);
  }

  // Ensures input-epsilon free transducers fst1 and fst2 have the
  // same domain and that for each string pair '(is, os)' in fst1,
  // '(is, os)' is the minimum weight match to 'is' in fst2.
  template <class A>
  bool MinRelated(const Fst<A> &fst1, const Fst<A> &fst2) {
    // Same domain
    VectorFst<Arc> P1(fst1), P2(fst2);
    Project(&P1, ProjectType::INPUT);
    Project(&P2, ProjectType::INPUT);
    if (!Equiv(P1, P2)) {
      LOG(ERROR) << "Inputs not equivalent";
      return false;
    }

    // Ensures seed used once per instantiation.
    static const UniformArcSelector<A> uniform_selector(seed_);
    const RandGenOptions<UniformArcSelector<A>> opts(uniform_selector,
                                                     kRandomPathLength);

    VectorFst<Arc> path, paths1, paths2;
    for (ssize_t n = 0; n < kNumRandomPaths; ++n) {
      RandGen(fst1, &path, opts);
      Invert(&path);
      ArcMap(&path, RmWeightMapper<Arc>());
      Compose(path, fst2, &paths1);
      Weight sum1 = ShortestDistance(paths1);
      Compose(paths1, path, &paths2);
      Weight sum2 = ShortestDistance(paths2);
      if (!ApproxEqual(Plus(sum1, sum2), sum2, kTestDelta)) {
        LOG(ERROR) << "Sums not equivalent: " << sum1 << " " << sum2;
        return false;
      }
    }
    return true;
  }

  // Tests ShortestDistance(A - P) >= ShortestDistance(A) times Threshold.
  template <class A>
  bool PruneEquiv(const Fst<A> &fst, const Fst<A> &pfst, Weight threshold) {
    VLOG(1) << "Check FSTs for sanity (including property bits).";
    FST_CHECK(Verify(fst));
    FST_CHECK(Verify(pfst));

    DifferenceFst<Arc> D(fst, DeterminizeFst<Arc>(RmEpsilonFst<Arc>(
                                  ArcMapFst(pfst, RmWeightMapper<Arc>()))));
    const Weight sum1 = Times(ShortestDistance(fst), threshold);
    const Weight sum2 = ShortestDistance(D);
    return ApproxEqual(Plus(sum1, sum2), sum1, kTestDelta);
  }

  // Random seed.
  uint64_t seed_;
  // Random state (for randomness in this class).
  std::mt19937_64 rand_;
  // FST with no states
  VectorFst<Arc> zero_fst_;
  // FST with one state that accepts epsilon.
  VectorFst<Arc> one_fst_;
  // FST with one state that accepts all strings.
  VectorFst<Arc> univ_fst_;
  // Generates weights used in testing.
  WeightGenerator generate_;
  // Maximum random path length.
  static constexpr int kRandomPathLength = 25;
  // Number of random paths to explore.
  static constexpr int kNumRandomPaths = 100;
  // Maximum number of nshortest paths.
  static constexpr int kNumRandomShortestPaths = 100;
  // Maximum number of nshortest states.
  static constexpr int kNumShortestStates = 10000;
  // Delta for equivalence tests.
  static constexpr float kTestDelta = .05;

  WeightedTester(const WeightedTester &) = delete;
  WeightedTester &operator=(const WeightedTester &) = delete;
};

// This class tests a variety of identities and properties that must
// hold for various algorithms on unweighted FSAs and that are not tested
// by WeightedTester. Only the specialization does anything interesting.
template <class Arc>
class UnweightedTester {
 public:
  UnweightedTester(const Fst<Arc> &zero_fsa, const Fst<Arc> &one_fsa,
                   const Fst<Arc> &univ_fsa, uint64_t seed) {}

  void Test(const Fst<Arc> &A1, const Fst<Arc> &A2, const Fst<Arc> &A3) {}
};

// Specialization for StdArc. This should work for any commutative,
// idempotent semiring when restricted to the unweighted case
// (being isomorphic to the boolean semiring).
template <>
class UnweightedTester<StdArc> {
 public:
  using Arc = StdArc;
  using Label = Arc::Label;
  using StateId = Arc::StateId;
  using Weight = Arc::Weight;

  UnweightedTester(const Fst<Arc> &zero_fsa, const Fst<Arc> &one_fsa,
                   const Fst<Arc> &univ_fsa, uint64_t seed)
      : zero_fsa_(zero_fsa),
        one_fsa_(one_fsa),
        univ_fsa_(univ_fsa),
        rand_(seed) {}

  void Test(const Fst<Arc> &A1, const Fst<Arc> &A2, const Fst<Arc> &A3) {
    TestRational(A1, A2, A3);
    TestIntersect(A1, A2, A3);
    TestOptimize(A1);
  }

 private:
  // Tests rational operations with identities.
  void TestRational(const Fst<Arc> &A1, const Fst<Arc> &A2,
                    const Fst<Arc> &A3) {
    {
      VLOG(1) << "Check the union contains its arguments (destructive).";
      VectorFst<Arc> U(A1);
      Union(&U, A2);

      FST_CHECK(Subset(A1, U));
      FST_CHECK(Subset(A2, U));
    }

    {
      VLOG(1) << "Check the union contains its arguments (delayed).";
      UnionFst<Arc> U(A1, A2);

      FST_CHECK(Subset(A1, U));
      FST_CHECK(Subset(A2, U));
    }

    {
      VLOG(1) << "Check if A^n c A* (destructive).";
      VectorFst<Arc> C(one_fsa_);
      const int n = std::uniform_int_distribution<>(0, 4)(rand_);
      for (int i = 0; i < n; ++i) Concat(&C, A1);

      VectorFst<Arc> S(A1);
      Closure(&S, CLOSURE_STAR);
      FST_CHECK(Subset(C, S));
    }

    {
      VLOG(1) << "Check if A^n c A* (delayed).";
      const int n = std::uniform_int_distribution<>(0, 4)(rand_);
      std::unique_ptr<Fst<Arc>> C = std::make_unique<VectorFst<Arc>>(one_fsa_);
      for (int i = 0; i < n; ++i) {
        C = std::make_unique<ConcatFst<Arc>>(*C, A1);
      }
      ClosureFst<Arc> S(A1, CLOSURE_STAR);
      FST_CHECK(Subset(*C, S));
    }
  }

  // Tests intersect-based operations.
  void TestIntersect(const Fst<Arc> &A1, const Fst<Arc> &A2,
                     const Fst<Arc> &A3) {
    VectorFst<Arc> S1(A1);
    VectorFst<Arc> S2(A2);
    VectorFst<Arc> S3(A3);

    ILabelCompare<Arc> comp;

    ArcSort(&S1, comp);
    ArcSort(&S2, comp);
    ArcSort(&S3, comp);

    {
      VLOG(1) << "Check the intersection is contained in its arguments.";
      IntersectFst<Arc> I1(S1, S2);
      FST_CHECK(Subset(I1, S1));
      FST_CHECK(Subset(I1, S2));
    }

    {
      VLOG(1) << "Check union distributes over intersection.";
      IntersectFst<Arc> I1(S1, S2);
      UnionFst<Arc> U1(I1, S3);

      UnionFst<Arc> U2(S1, S3);
      UnionFst<Arc> U3(S2, S3);
      ArcSortFst<Arc, ILabelCompare<Arc>> S4(U3, comp);
      IntersectFst<Arc> I2(U2, S4);

      FST_CHECK(Equiv(U1, I2));
    }

    VectorFst<Arc> C1;
    VectorFst<Arc> C2;
    Complement(S1, &C1);
    Complement(S2, &C2);
    ArcSort(&C1, comp);
    ArcSort(&C2, comp);

    {
      VLOG(1) << "Check S U S' = Sigma*";
      UnionFst<Arc> U(S1, C1);
      FST_CHECK(Equiv(U, univ_fsa_));
    }

    {
      VLOG(1) << "Check S n S' = {}";
      IntersectFst<Arc> I(S1, C1);
      FST_CHECK(Equiv(I, zero_fsa_));
    }

    {
      VLOG(1) << "Check (S1' U S2') == (S1 n S2)'";
      UnionFst<Arc> U(C1, C2);

      IntersectFst<Arc> I(S1, S2);
      VectorFst<Arc> C3;
      Complement(I, &C3);
      FST_CHECK(Equiv(U, C3));
    }

    {
      VLOG(1) << "Check (S1' n S2') == (S1 U S2)'";
      IntersectFst<Arc> I(C1, C2);

      UnionFst<Arc> U(S1, S2);
      VectorFst<Arc> C3;
      Complement(U, &C3);
      FST_CHECK(Equiv(I, C3));
    }
  }

  // Tests optimization operations.
  void TestOptimize(const Fst<Arc> &A) {
    {
      VLOG(1) << "Check determinized FSA is equivalent to its input.";
      DeterminizeFst<Arc> D(A);
      FST_CHECK(Equiv(A, D));
    }

    {
      VLOG(1) << "Check disambiguated FSA is equivalent to its input.";
      VectorFst<Arc> R(A), D;
      RmEpsilon(&R);

      Disambiguate(R, &D);
      FST_CHECK(Equiv(R, D));
    }

    {
      VLOG(1) << "Check minimized FSA is equivalent to its input.";
      int n;
      {
        RmEpsilonFst<Arc> R(A);
        DeterminizeFst<Arc> D(R);
        VectorFst<Arc> M(D);
        Minimize(&M, static_cast<MutableFst<Arc> *>(nullptr), kDelta);
        FST_CHECK(Equiv(A, M));
        n = M.NumStates();
      }

      if (n) {  // Skips test if A is the empty machine.
        VLOG(1) << "Check that Hopcroft's and Revuz's algorithms lead to the"
                << " same number of states as Brozozowski's algorithm";
        VectorFst<Arc> R;
        Reverse(A, &R);
        RmEpsilon(&R);
        DeterminizeFst<Arc> DR(R);
        VectorFst<Arc> RD;
        Reverse(DR, &RD);
        DeterminizeFst<Arc> DRD(RD);
        VectorFst<Arc> M(DRD);
        FST_CHECK_EQ(n + 1, M.NumStates());  // Accounts for the epsilon transition
                                         // to the initial state.
      }
    }
  }

  // Tests if two FSAS are equivalent.
  bool Equiv(const Fst<Arc> &fsa1, const Fst<Arc> &fsa2) {
    VLOG(1) << "Check FSAs for sanity (including property bits).";
    FST_CHECK(Verify(fsa1));
    FST_CHECK(Verify(fsa2));

    VectorFst<Arc> vfsa1(fsa1);
    VectorFst<Arc> vfsa2(fsa2);
    RmEpsilon(&vfsa1);
    RmEpsilon(&vfsa2);
    DeterminizeFst<Arc> dfa1(vfsa1);
    DeterminizeFst<Arc> dfa2(vfsa2);

    // Test equivalence using union-find algorithm
    bool equiv1 = Equivalent(dfa1, dfa2);

    // Test equivalence by checking if (S1 - S2) U (S2 - S1) is empty
    ILabelCompare<Arc> comp;
    VectorFst<Arc> sdfa1(dfa1);
    ArcSort(&sdfa1, comp);
    VectorFst<Arc> sdfa2(dfa2);
    ArcSort(&sdfa2, comp);

    DifferenceFst<Arc> dfsa1(sdfa1, sdfa2);
    DifferenceFst<Arc> dfsa2(sdfa2, sdfa1);

    VectorFst<Arc> ufsa(dfsa1);
    Union(&ufsa, dfsa2);
    Connect(&ufsa);
    bool equiv2 = ufsa.NumStates() == 0;

    // Checks both equivalence tests match.
    FST_CHECK((equiv1 && equiv2) || (!equiv1 && !equiv2));

    return equiv1;
  }

  // Tests if FSA1 is a subset of FSA2 (disregarding weights).
  bool Subset(const Fst<Arc> &fsa1, const Fst<Arc> &fsa2) {
    VLOG(1) << "Check FSAs (incl. property bits) for sanity";
    FST_CHECK(Verify(fsa1));
    FST_CHECK(Verify(fsa2));

    VectorFst<StdArc> vfsa1;
    VectorFst<StdArc> vfsa2;
    RmEpsilon(&vfsa1);
    RmEpsilon(&vfsa2);
    ILabelCompare<StdArc> comp;
    ArcSort(&vfsa1, comp);
    ArcSort(&vfsa2, comp);
    IntersectFst<StdArc> ifsa(vfsa1, vfsa2);
    DeterminizeFst<StdArc> dfa1(vfsa1);
    DeterminizeFst<StdArc> dfa2(ifsa);
    return Equivalent(dfa1, dfa2);
  }

  // Returns complement FSA.
  void Complement(const Fst<Arc> &ifsa, MutableFst<Arc> *ofsa) {
    RmEpsilonFst<Arc> rfsa(ifsa);
    DeterminizeFst<Arc> dfa(rfsa);
    DifferenceFst<Arc> cfsa(univ_fsa_, dfa);
    *ofsa = cfsa;
  }

  // FSA with no states.
  VectorFst<Arc> zero_fsa_;
  // FSA with one state that accepts epsilon.
  VectorFst<Arc> one_fsa_;
  // FSA with one state that accepts all strings.
  VectorFst<Arc> univ_fsa_;
  // Random state.
  std::mt19937_64 rand_;
};

// This class tests a variety of identities and properties that must
// hold for various FST algorithms. It randomly generates FSTs, using
// function object 'weight_generator' to select weights. 'WeightTester'
// and 'UnweightedTester' are then called.
template <class Arc>
class AlgoTester {
 public:
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;
  using WeightGenerator = WeightGenerate<Weight>;

  AlgoTester(WeightGenerator generator, uint64_t seed)
      : generate_(std::move(generator)), rand_(seed) {
    one_fst_.AddState();
    one_fst_.SetStart(0);
    one_fst_.SetFinal(0);

    univ_fst_.AddState();
    univ_fst_.SetStart(0);
    univ_fst_.SetFinal(0);
    for (int i = 0; i < kNumRandomLabels; ++i) univ_fst_.EmplaceArc(0, i, i, 0);

    weighted_tester_.reset(new WeightedTester<Arc>(seed, zero_fst_, one_fst_,
                                                   univ_fst_, generate_));

    unweighted_tester_.reset(
        new UnweightedTester<Arc>(zero_fst_, one_fst_, univ_fst_, seed));
  }

  void MakeRandFst(MutableFst<Arc> *fst) {
    RandFst<Arc, WeightGenerator>(kNumRandomStates, kNumRandomArcs,
                                  kNumRandomLabels, kAcyclicProb, generate_,
                                  rand_(), fst);
  }

  void Test() {
    VLOG(1) << "weight type = " << Weight::Type();

    for (int i = 0; i < FST_FLAGS_repeat; ++i) {
      // Random transducers
      VectorFst<Arc> T1;
      VectorFst<Arc> T2;
      VectorFst<Arc> T3;
      MakeRandFst(&T1);
      MakeRandFst(&T2);
      MakeRandFst(&T3);
      weighted_tester_->Test(T1, T2, T3);

      VectorFst<Arc> A1(T1);
      VectorFst<Arc> A2(T2);
      VectorFst<Arc> A3(T3);
      Project(&A1, ProjectType::OUTPUT);
      Project(&A2, ProjectType::INPUT);
      Project(&A3, ProjectType::INPUT);
      ArcMap(&A1, rm_weight_mapper_);
      ArcMap(&A2, rm_weight_mapper_);
      ArcMap(&A3, rm_weight_mapper_);
      unweighted_tester_->Test(A1, A2, A3);
    }
  }

 private:
  // Generates weights used in testing.
  WeightGenerator generate_;
  // Random state used to seed RandFst.
  std::mt19937_64 rand_;
  // FST with no states
  VectorFst<Arc> zero_fst_;
  // FST with one state that accepts epsilon.
  VectorFst<Arc> one_fst_;
  // FST with one state that accepts all strings.
  VectorFst<Arc> univ_fst_;
  // Tests weighted FSTs
  std::unique_ptr<WeightedTester<Arc>> weighted_tester_;
  // Tests unweighted FSTs
  std::unique_ptr<UnweightedTester<Arc>> unweighted_tester_;
  // Mapper to remove weights from an Fst
  RmWeightMapper<Arc> rm_weight_mapper_;
  // Maximum number of states in random test Fst.
  static constexpr int kNumRandomStates = 10;
  // Maximum number of arcs in random test Fst.
  static constexpr int kNumRandomArcs = 25;
  // Number of alternative random labels.
  static constexpr int kNumRandomLabels = 5;
  // Probability to force an acyclic Fst
  static constexpr float kAcyclicProb = .25;
  // Maximum random path length.
  static constexpr int kRandomPathLength = 25;
  // Number of random paths to explore.
  static constexpr int kNumRandomPaths = 100;

  AlgoTester(const AlgoTester &) = delete;
  AlgoTester &operator=(const AlgoTester &) = delete;
};
}  // namespace fst

#endif  // FST_TEST_ALGO_TEST_H_
