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
// Utility class for regression testing of FST weights.

#ifndef FST_TEST_WEIGHT_TESTER_H_
#define FST_TEST_WEIGHT_TESTER_H_

#include <sstream>
#include <utility>

#include <fst/log.h>
#include <fst/weight.h>

namespace fst {

// This class tests a variety of identities and properties that must
// hold for the Weight class to be well-defined. It calls function object
// WEIGHT_GENERATOR to select weights that are used in the tests.
template <class Weight, class WeightGenerator = WeightGenerate<Weight>>
class WeightTester {
 public:
  explicit WeightTester(WeightGenerator generator)
      : weight_generator_(std::move(generator)) {}

  void Test(int iterations) {
    for (int i = 0; i < iterations; ++i) {
      // Selects the test weights.
      const Weight w1(weight_generator_());
      const Weight w2(weight_generator_());
      const Weight w3(weight_generator_());

      VLOG(1) << "weight type = " << Weight::Type();
      VLOG(1) << "w1 = " << w1;
      VLOG(1) << "w2 = " << w2;
      VLOG(1) << "w3 = " << w3;

      TestSemiring(w1, w2, w3);
      TestDivision(w1, w2);
      TestReverse(w1, w2);
      TestEquality(w1, w2, w3);
      TestIO(w1);
      TestCopy(w1);
    }
  }

 private:
  // Note in the tests below we use ApproxEqual rather than == and add
  // kDelta to inequalities where the weights might be inexact.

  // Tests (Plus, Times, Zero, One) defines a commutative semiring.
  void TestSemiring(Weight w1, Weight w2, Weight w3) {
    // Checks that the operations are closed.
    FST_CHECK(Plus(w1, w2).Member());
    FST_CHECK(Times(w1, w2).Member());

    // Checks that the operations are associative.
    FST_CHECK(ApproxEqual(Plus(w1, Plus(w2, w3)), Plus(Plus(w1, w2), w3)));
    FST_CHECK(ApproxEqual(Times(w1, Times(w2, w3)), Times(Times(w1, w2), w3)));

    // Checks the identity elements.
    FST_CHECK(Plus(w1, Weight::Zero()) == w1);
    FST_CHECK(Plus(Weight::Zero(), w1) == w1);
    FST_CHECK(Times(w1, Weight::One()) == w1);
    FST_CHECK(Times(Weight::One(), w1) == w1);

    // Check the no weight element.
    FST_CHECK(!Weight::NoWeight().Member());
    FST_CHECK(!Plus(w1, Weight::NoWeight()).Member());
    FST_CHECK(!Plus(Weight::NoWeight(), w1).Member());
    FST_CHECK(!Times(w1, Weight::NoWeight()).Member());
    FST_CHECK(!Times(Weight::NoWeight(), w1).Member());

    // Checks that the operations commute.
    FST_CHECK(ApproxEqual(Plus(w1, w2), Plus(w2, w1)));

    if (Weight::Properties() & kCommutative)
      FST_CHECK(ApproxEqual(Times(w1, w2), Times(w2, w1)));

    // Checks Zero() is the annihilator.
    FST_CHECK(Times(w1, Weight::Zero()) == Weight::Zero());
    FST_CHECK(Times(Weight::Zero(), w1) == Weight::Zero());

    // Check Power(w, 0) is Weight::One()
    FST_CHECK(Power(w1, 0) == Weight::One());

    // Check Power(w, 1) is w
    FST_CHECK(Power(w1, 1) == w1);

    // Check Power(w, 2) is Times(w, w)
    FST_CHECK(Power(w1, 2) == Times(w1, w1));

    // Check Power(w, 3) is Times(w, Times(w, w))
    FST_CHECK(Power(w1, 3) == Times(w1, Times(w1, w1)));

    // Checks distributivity.
    if (Weight::Properties() & kLeftSemiring) {
      FST_CHECK(ApproxEqual(Times(w1, Plus(w2, w3)),
                        Plus(Times(w1, w2), Times(w1, w3))));
    }
    if (Weight::Properties() & kRightSemiring)
      FST_CHECK(ApproxEqual(Times(Plus(w1, w2), w3),
                        Plus(Times(w1, w3), Times(w2, w3))));

    if (Weight::Properties() & kIdempotent) FST_CHECK(Plus(w1, w1) == w1);

    if (Weight::Properties() & kPath)
      FST_CHECK(Plus(w1, w2) == w1 || Plus(w1, w2) == w2);

    // Ensure weights form a left or right semiring.
    FST_CHECK(Weight::Properties() & (kLeftSemiring | kRightSemiring));

    // Check when Times() is commutative that it is marked as a semiring.
    if (Weight::Properties() & kCommutative)
      FST_CHECK(Weight::Properties() & kSemiring);
  }

  // Tests division operation.
  void TestDivision(Weight w1, Weight w2) {
    Weight p = Times(w1, w2);
    VLOG(1) << "TestDivision: p = " << p;

    if (Weight::Properties() & kLeftSemiring) {
      Weight d = Divide(p, w1, DIVIDE_LEFT);
      if (d.Member()) FST_CHECK(ApproxEqual(p, Times(w1, d)));
      FST_CHECK(!Divide(w1, Weight::NoWeight(), DIVIDE_LEFT).Member());
      FST_CHECK(!Divide(Weight::NoWeight(), w1, DIVIDE_LEFT).Member());
    }

    if (Weight::Properties() & kRightSemiring) {
      Weight d = Divide(p, w2, DIVIDE_RIGHT);
      if (d.Member()) FST_CHECK(ApproxEqual(p, Times(d, w2)));
      FST_CHECK(!Divide(w1, Weight::NoWeight(), DIVIDE_RIGHT).Member());
      FST_CHECK(!Divide(Weight::NoWeight(), w1, DIVIDE_RIGHT).Member());
    }

    if (Weight::Properties() & kCommutative) {
      Weight d1 = Divide(p, w1, DIVIDE_ANY);
      if (d1.Member()) FST_CHECK(ApproxEqual(p, Times(d1, w1)));
      Weight d2 = Divide(p, w2, DIVIDE_ANY);
      if (d2.Member()) FST_CHECK(ApproxEqual(p, Times(w2, d2)));
    }
  }

  // Tests reverse operation.
  void TestReverse(Weight w1, Weight w2) {
    using ReverseWeight = typename Weight::ReverseWeight;

    ReverseWeight rw1 = w1.Reverse();
    ReverseWeight rw2 = w2.Reverse();

    FST_CHECK(rw1.Reverse() == w1);
    FST_CHECK(Plus(w1, w2).Reverse() == Plus(rw1, rw2));
    FST_CHECK(Times(w1, w2).Reverse() == Times(rw2, rw1));
  }

  // Tests == is an equivalence relation.
  void TestEquality(Weight w1, Weight w2, Weight w3) {
    // Checks reflexivity.
    FST_CHECK(w1 == w1);

    // Checks symmetry.
    FST_CHECK((w1 == w2) == (w2 == w1));

    // Checks transitivity.
    if (w1 == w2 && w2 == w3) FST_CHECK(w1 == w3);

    // Checks that two weights are either equal or not equal.
    FST_CHECK((w1 == w2) ^ (w1 != w2));

    if (w1 == w2) {
      // Checks that equal weights have identical hashes.
      FST_CHECK(w1.Hash() == w2.Hash());
      // Checks that equal weights are also approximately equal.
      FST_CHECK(ApproxEqual(w1, w2));
    }

    // Checks that weights which are not even approximately equal are also
    // strictly unequal.
    if (!ApproxEqual(w1, w2)) {
      FST_CHECK(w1 != w2);
    }
  }

  // Tests binary serialization and textual I/O.
  void TestIO(Weight w) {
    // Tests binary I/O
    {
      std::ostringstream os;
      w.Write(os);
      os.flush();
      std::istringstream is(os.str());
      Weight v;
      v.Read(is);
      FST_CHECK_EQ(w, v);
    }

    // Tests textual I/O.
    {
      std::ostringstream os;
      os << w;
      std::istringstream is(os.str());
      Weight v(Weight::One());
      is >> v;
      FST_CHECK(ApproxEqual(w, v));
    }
  }

  // Tests copy constructor and assignment operator
  void TestCopy(Weight w) {
    Weight x = w;
    FST_CHECK(w == x);

    x = Weight(w);
    FST_CHECK(w == x);

    x.operator=(x);
    FST_CHECK(w == x);
  }

  // Generates weights used in testing.
  WeightGenerator weight_generator_;
};

}  // namespace fst

#endif  // FST_TEST_WEIGHT_TESTER_H_
