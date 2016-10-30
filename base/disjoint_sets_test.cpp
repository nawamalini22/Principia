#include "base/disjoint_sets.hpp"

#include <vector>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

using testing::Eq;

namespace principia {
namespace base {

namespace {

struct CountableInteger;

}  // namespace

template<>
class SubsetProperties<CountableInteger> {
 public:
  void MergeWith(SubsetProperties& other) {
    cardinality += other.cardinality;
    other.cardinality = -1;
  }

  int cardinality = 1;
};

namespace {

struct Integer {
  int value;
  SubsetNode<Integer> subset_node;
};

struct CountableInteger {
  int value;
  SubsetNode<CountableInteger> subset_node;
};

}  // namespace

class DisjointSetsTest : public testing::Test {
 protected:
  DisjointSetsTest() {
    integers_.resize(50);
    countable_integers_.resize(50);
    for(int i = 0; i < 50; ++i) {
      integers_[i].value = i;
      countable_integers_[i].value = i;
      MakeSingleton<Integer>(&integers_[i]);
      MakeSingleton<CountableInteger>(&countable_integers_[i]);
    }
  }

  std::vector<Integer> integers_;
  std::vector<CountableInteger> countable_integers_;
};

template<>
not_null<SubsetNode<Integer>*> GetSubsetNode<Integer>(Integer& element) {
  return &element.subset_node;
}

template<>
not_null<SubsetNode<CountableInteger>*> GetSubsetNode<CountableInteger>(
    CountableInteger& element) {
  return &element.subset_node;
}

TEST_F(DisjointSetsTest, Congruence) {
  for (auto& left : integers_) {
    for (auto& right : integers_) {
      if (left.value % 5 == right.value % 5) {
        auto const unified = Unite(Find(left), Find(right));
        EXPECT_EQ(unified, Find(left));
        EXPECT_EQ(unified, Find(right));
      }
    }
  }
  for (auto& i : integers_) {
    EXPECT_EQ(Find(integers_[i.value % 5]), Find(i));
  }

  for (auto& left : countable_integers_) {
    for (auto& right : countable_integers_) {
      if (left.value % 5 == right.value % 5) {
        auto const unified = Unite(Find(left), Find(right));
        EXPECT_EQ(unified, Find(left));
        EXPECT_EQ(unified, Find(right));
      }
    }
  }
  for (auto& i : countable_integers_) {
    EXPECT_EQ(10, Find(i).properties().cardinality);
  }
}

}  // namespace base
}  // namespace principia
