#include "trajectory.hpp"

#include <list>
#include <map>
#include <string>

#include "body.hpp"
#include "geometry/grassmann.hpp"
#include "geometry/named_quantities.hpp"
#include "geometry/point.hpp"
#include "geometry/r3_element.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "quantities/quantities.hpp"
#include "quantities/si.hpp"
#include "testing_utilities/death_message.hpp"

using principia::geometry::Instant;
using principia::geometry::Point;
using principia::geometry::R3Element;
using principia::geometry::Vector;
using principia::quantities::Length;
using principia::quantities::Mass;
using principia::quantities::Speed;
using principia::quantities::SIUnit;
using principia::si::Metre;
using principia::si::Second;
using principia::testing_utilities::DeathMessage;
using testing::ElementsAre;
using testing::Eq;
using testing::Pair;
using testing::Ref;

namespace principia {
namespace physics {

class World;

class TrajectoryTest : public testing::Test {
 protected:
  void SetUp() override {
    // TODO(phl): See how we can simplify these horrendous expressions.
    q1_ = Position<World>(
        Vector<Length, World>({1 * Metre, 2 * Metre, 3 * Metre}));
    q2_ = Position<World>(
        Vector<Length, World>({11 * Metre, 12 * Metre, 13 * Metre}));
    q3_ = Position<World>(
        Vector<Length, World>({21 * Metre, 22 * Metre, 23 * Metre}));
    q4_ = Position<World>(
        Vector<Length, World>({31 * Metre, 32 * Metre, 33 * Metre}));
    p1_ = Velocity<World>({4 * Metre / Second,
                           5 * Metre / Second,
                           6 * Metre / Second});
    p2_ = Velocity<World>({14 * Metre / Second,
                           15 * Metre / Second,
                           16 * Metre / Second});
    p3_ = Velocity<World>({24 * Metre / Second,
                           25 * Metre / Second,
                           26 * Metre / Second});
    p4_ = Velocity<World>({34 * Metre / Second,
                           35 * Metre / Second,
                           36 * Metre / Second});
    d1_ = std::make_unique<DegreesOfFreedom<World>>(q1_, p1_);
    d2_ = std::make_unique<DegreesOfFreedom<World>>(q2_, p2_);
    d3_ = std::make_unique<DegreesOfFreedom<World>>(q3_, p3_);
    d4_ = std::make_unique<DegreesOfFreedom<World>>(q4_, p4_);
    t0_ = Instant(0 * Second);
    t1_ = t0_ + 7 * Second;
    t2_ = t0_ + 17 * Second;
    t3_ = t0_ + 27 * Second;
    t4_ = t0_ + 37 * Second;

    massive_body_.reset(new Body<World>(1 * SIUnit<Mass>()));
    massless_body_.reset(new Body<World>(0 * SIUnit<Mass>()));
    massive_trajectory_.reset(new Trajectory<World>(*massive_body_));
    massless_trajectory_.reset(new Trajectory<World>(*massless_body_));
  }

  Position<World> q1_, q2_, q3_, q4_;
  Velocity<World> p1_, p2_, p3_, p4_;
  std::unique_ptr<DegreesOfFreedom<World>> d1_, d2_, d3_, d4_;
  Instant t0_, t1_, t2_, t3_, t4_;
  std::unique_ptr<Body<World>> massive_body_;
  std::unique_ptr<Body<World>> massless_body_;
  std::unique_ptr<Trajectory<World>> massive_trajectory_;
  std::unique_ptr<Trajectory<World>> massless_trajectory_;
};

using TrajectoryDeathTest = TrajectoryTest;

TEST_F(TrajectoryDeathTest, AppendError) {
  EXPECT_DEATH({
    massive_trajectory_->Append(t2_, *d2_);
    massive_trajectory_->Append(t1_, *d1_);
  }, DeathMessage("out of order"));
  EXPECT_DEATH({
    massive_trajectory_->Append(t1_, *d1_);
    massive_trajectory_->Append(t1_, *d1_);
  }, DeathMessage("existing time"));
}

TEST_F(TrajectoryTest, AppendSuccess) {
  massive_trajectory_->Append(t1_, *d1_);
  massive_trajectory_->Append(t2_, *d2_);
  massive_trajectory_->Append(t3_, *d3_);
  std::map<Instant, Position<World>> const positions =
      massive_trajectory_->Positions();
  std::map<Instant, Velocity<World>> const velocities =
      massive_trajectory_->Velocities();
  std::list<Instant> const times = massive_trajectory_->Times();
  EXPECT_THAT(positions,
              ElementsAre(Pair(t1_, q1_), Pair(t2_, q2_), Pair(t3_, q3_)));
  EXPECT_THAT(velocities,
              ElementsAre(Pair(t1_, p1_), Pair(t2_, p2_), Pair(t3_, p3_)));
  EXPECT_THAT(times, ElementsAre(t1_, t2_, t3_));
  EXPECT_THAT(massive_trajectory_->body(), Ref(*massive_body_));
}

TEST_F(TrajectoryDeathTest, ForkError) {
  EXPECT_DEATH({
    massive_trajectory_->Append(t1_, *d1_);
    massive_trajectory_->Append(t3_, *d3_);
    Trajectory<World>* fork = massive_trajectory_->Fork(t2_);
  }, DeathMessage("nonexistent time"));
}

TEST_F(TrajectoryTest, ForkSuccess) {
  massive_trajectory_->Append(t1_, *d1_);
  massive_trajectory_->Append(t2_, *d2_);
  massive_trajectory_->Append(t3_, *d3_);
  Trajectory<World>* fork = massive_trajectory_->Fork(t2_);
  fork->Append(t4_, *d4_);
  std::map<Instant, Position<World>> positions =
      massive_trajectory_->Positions();
  std::map<Instant, Velocity<World>> velocities =
      massive_trajectory_->Velocities();
  std::list<Instant> times = massive_trajectory_->Times();
  EXPECT_THAT(positions,
              ElementsAre(Pair(t1_, q1_), Pair(t2_, q2_), Pair(t3_, q3_)));
  EXPECT_THAT(velocities,
              ElementsAre(Pair(t1_, p1_), Pair(t2_, p2_), Pair(t3_, p3_)));
  EXPECT_THAT(times, ElementsAre(t1_, t2_, t3_));
  EXPECT_THAT(fork->body(), Ref(*massive_body_));
  positions = fork->Positions();
  velocities = fork->Velocities();
  times = fork->Times();
  EXPECT_THAT(positions,
              ElementsAre(Pair(t1_, q1_), Pair(t2_, q2_),
                          Pair(t3_, q3_), Pair(t4_, q4_)));
  EXPECT_THAT(velocities,
              ElementsAre(Pair(t1_, p1_), Pair(t2_, p2_),
                          Pair(t3_, p3_), Pair(t4_, p4_)));
  EXPECT_THAT(times, ElementsAre(t1_, t2_, t3_, t4_));
  EXPECT_THAT(fork->body(), Ref(*massive_body_));
}

TEST_F(TrajectoryDeathTest, DeleteForkError) {
  EXPECT_DEATH({
    massive_trajectory_->DeleteFork(nullptr);
  }, DeathMessage("'fork'.* non NULL"));
  EXPECT_DEATH({
    massive_trajectory_->Append(t1_, *d1_);
    Trajectory<World>* root = massive_trajectory_.get();
    massive_trajectory_->DeleteFork(&root);
  }, DeathMessage("'fork_time'.* non NULL"));
  EXPECT_DEATH({
    massive_trajectory_->Append(t1_, *d1_);
    Trajectory<World>* fork1 = massive_trajectory_->Fork(t1_);
    fork1->Append(t2_, *d2_);
    Trajectory<World>* fork2 = fork1->Fork(t2_);
    massive_trajectory_->DeleteFork(&fork2);
  }, DeathMessage("not a child"));
}

TEST_F(TrajectoryTest, DeleteForkSuccess) {
  massive_trajectory_->Append(t1_, *d1_);
  massive_trajectory_->Append(t2_, *d2_);
  massive_trajectory_->Append(t3_, *d3_);
  Trajectory<World>* fork1 = massive_trajectory_->Fork(t2_);
  Trajectory<World>* fork2 = massive_trajectory_->Fork(t2_);
  fork1->Append(t4_, *d4_);
  massive_trajectory_->DeleteFork(&fork2);
  EXPECT_EQ(nullptr, fork2);
  std::map<Instant, Position<World>> positions =
      massive_trajectory_->Positions();
  std::map<Instant, Velocity<World>> velocities =
      massive_trajectory_->Velocities();
  std::list<Instant> times = massive_trajectory_->Times();
  EXPECT_THAT(positions,
              ElementsAre(Pair(t1_, q1_), Pair(t2_, q2_), Pair(t3_, q3_)));
  EXPECT_THAT(velocities,
              ElementsAre(Pair(t1_, p1_), Pair(t2_, p2_), Pair(t3_, p3_)));
  EXPECT_THAT(times, ElementsAre(t1_, t2_, t3_));
  EXPECT_THAT(fork1->body(), Ref(*massive_body_));
  positions = fork1->Positions();
  velocities = fork1->Velocities();
  times = fork1->Times();
  EXPECT_THAT(positions,
              ElementsAre(Pair(t1_, q1_), Pair(t2_, q2_),
                          Pair(t3_, q3_), Pair(t4_, q4_)));
  EXPECT_THAT(velocities,
              ElementsAre(Pair(t1_, p1_), Pair(t2_, p2_),
                          Pair(t3_, p3_), Pair(t4_, p4_)));
  EXPECT_THAT(times, ElementsAre(t1_, t2_, t3_, t4_));
  EXPECT_THAT(fork1->body(), Ref(*massive_body_));
  massive_trajectory_.reset();
}

TEST_F(TrajectoryDeathTest, LastError) {
  EXPECT_DEATH({
    massive_trajectory_->last_position();
  }, DeathMessage("Empty trajectory"));
  EXPECT_DEATH({
    massive_trajectory_->last_velocity();
  }, DeathMessage("Empty trajectory"));
  EXPECT_DEATH({
    massive_trajectory_->last_time();
  }, DeathMessage("Empty trajectory"));
}

TEST_F(TrajectoryTest, LastSuccess) {
  massive_trajectory_->Append(t1_, *d1_);
  massive_trajectory_->Append(t2_, *d2_);
  massive_trajectory_->Append(t3_, *d3_);
  EXPECT_EQ(q3_, massive_trajectory_->last_position());
  EXPECT_EQ(p3_, massive_trajectory_->last_velocity());
  EXPECT_EQ(t3_, massive_trajectory_->last_time());
}

TEST_F(TrajectoryTest, Root) {
  massive_trajectory_->Append(t1_, *d1_);
  massive_trajectory_->Append(t2_, *d2_);
  massive_trajectory_->Append(t3_, *d3_);
  Trajectory<World>* fork = massive_trajectory_->Fork(t2_);
  EXPECT_TRUE(massive_trajectory_->is_root());
  EXPECT_FALSE(fork->is_root());
  EXPECT_EQ(massive_trajectory_.get(), massive_trajectory_->root());
  EXPECT_EQ(massive_trajectory_.get(), fork->root());
  EXPECT_EQ(nullptr, massive_trajectory_->fork_time());
  EXPECT_EQ(t2_, *fork->fork_time());
}

TEST_F(TrajectoryDeathTest, ForgetAfterError) {
  EXPECT_DEATH({
    massive_trajectory_->Append(t1_, *d1_);
    massive_trajectory_->ForgetAfter(t2_);
  }, DeathMessage("nonexistent time.* root"));
  EXPECT_DEATH({
    massive_trajectory_->Append(t1_, *d1_);
    Trajectory<World>* fork = massive_trajectory_->Fork(t1_);
    fork->ForgetAfter(t2_);
  }, DeathMessage("nonexistent time.* nonroot"));
}

TEST_F(TrajectoryTest, ForgetAfterSuccess) {
  massive_trajectory_->Append(t1_, *d1_);
  massive_trajectory_->Append(t2_, *d2_);
  massive_trajectory_->Append(t3_, *d3_);
  Trajectory<World>* fork = massive_trajectory_->Fork(t2_);
  fork->Append(t4_, *d4_);

  fork->ForgetAfter(t3_);
  std::map<Instant, Position<World>> positions = fork->Positions();
  std::map<Instant, Velocity<World>> velocities = fork->Velocities();
  std::list<Instant> times = fork->Times();
  EXPECT_THAT(positions,
              ElementsAre(Pair(t1_, q1_), Pair(t2_, q2_), Pair(t3_, q3_)));
  EXPECT_THAT(velocities,
              ElementsAre(Pair(t1_, p1_), Pair(t2_, p2_), Pair(t3_, p3_)));
  EXPECT_THAT(times, ElementsAre(t1_, t2_, t3_));

  fork->ForgetAfter(t2_);
  positions = fork->Positions();
  velocities = fork->Velocities();
  times = fork->Times();
  EXPECT_THAT(positions, ElementsAre(Pair(t1_, q1_), Pair(t2_, q2_)));
  EXPECT_THAT(velocities, ElementsAre(Pair(t1_, p1_), Pair(t2_, p2_)));
  EXPECT_THAT(times, ElementsAre(t1_, t2_));
  EXPECT_EQ(q2_, fork->last_position());
  EXPECT_EQ(p2_, fork->last_velocity());
  EXPECT_EQ(t2_, fork->last_time());

  positions = massive_trajectory_->Positions();
  velocities = massive_trajectory_->Velocities();
  times = massive_trajectory_->Times();
  EXPECT_THAT(positions,
              ElementsAre(Pair(t1_, q1_), Pair(t2_, q2_), Pair(t3_, q3_)));
  EXPECT_THAT(velocities,
              ElementsAre(Pair(t1_, p1_), Pair(t2_, p2_), Pair(t3_, p3_)));
  EXPECT_THAT(times, ElementsAre(t1_, t2_, t3_));

  massive_trajectory_->ForgetAfter(t1_);
  positions = massive_trajectory_->Positions();
  velocities = massive_trajectory_->Velocities();
  times = massive_trajectory_->Times();
  EXPECT_THAT(positions, ElementsAre(Pair(t1_, q1_)));
  EXPECT_THAT(velocities, ElementsAre(Pair(t1_, p1_)));
  EXPECT_THAT(times, ElementsAre(t1_));
  // Don't use fork, it is dangling.
}

TEST_F(TrajectoryDeathTest, ForgetBeforeError) {
  EXPECT_DEATH({
    massive_trajectory_->Append(t1_, *d1_);
    Trajectory<World>* fork = massive_trajectory_->Fork(t1_);
    fork->ForgetBefore(t1_);
  }, DeathMessage("nonroot"));
  EXPECT_DEATH({
    massive_trajectory_->Append(t1_, *d1_);
    massive_trajectory_->ForgetBefore(t2_);
  }, DeathMessage("nonexistent time"));
}

TEST_F(TrajectoryTest, ForgetBeforeSuccess) {
  massive_trajectory_->Append(t1_, *d1_);
  massive_trajectory_->Append(t2_, *d2_);
  massive_trajectory_->Append(t3_, *d3_);
  Trajectory<World>* fork = massive_trajectory_->Fork(t2_);
  fork->Append(t4_, *d4_);

  massive_trajectory_->ForgetBefore(t1_);
  std::map<Instant, Position<World>> positions =
      massive_trajectory_->Positions();
  std::map<Instant, Velocity<World>> velocities =
      massive_trajectory_->Velocities();
  std::list<Instant> times = massive_trajectory_->Times();
  EXPECT_THAT(positions, ElementsAre(Pair(t2_, q2_), Pair(t3_, q3_)));
  EXPECT_THAT(velocities, ElementsAre(Pair(t2_, p2_), Pair(t3_, p3_)));
  EXPECT_THAT(times, ElementsAre(t2_, t3_));
  positions = fork->Positions();
  velocities = fork->Velocities();
  times = fork->Times();
  EXPECT_THAT(positions,
              ElementsAre(Pair(t2_, q2_), Pair(t3_, q3_), Pair(t4_, q4_)));
  EXPECT_THAT(velocities,
              ElementsAre(Pair(t2_, p2_), Pair(t3_, p3_), Pair(t4_, p4_)));
  EXPECT_THAT(times, ElementsAre(t2_, t3_, t4_));

  massive_trajectory_->ForgetBefore(t2_);
  positions = massive_trajectory_->Positions();
  velocities = massive_trajectory_->Velocities();
  times = massive_trajectory_->Times();
  EXPECT_THAT(positions, ElementsAre(Pair(t3_, q3_)));
  EXPECT_THAT(velocities, ElementsAre(Pair(t3_, p3_)));
  EXPECT_THAT(times, ElementsAre(t3_));
  // Don't use fork, it is dangling.
}

TEST_F(TrajectoryDeathTest, IntrinsicAccelerationError) {
  EXPECT_DEATH({
    massive_trajectory_->set_intrinsic_acceleration(
        [](Instant const& t) { return Vector<Acceleration, World>(); } );
  }, DeathMessage("massive body"));
  EXPECT_DEATH({
    massless_trajectory_->set_intrinsic_acceleration(
        [](Instant const& t) { return Vector<Acceleration, World>(); } );
    massless_trajectory_->set_intrinsic_acceleration(
        [](Instant const& t) { return Vector<Acceleration, World>(); } );
  }, DeathMessage("already has.* acceleration"));
}

TEST_F(TrajectoryDeathTest, IntrinsicAccelerationSuccess) {
  massless_trajectory_->Append(t1_, *d1_);
  massless_trajectory_->Append(t2_, *d2_);
  massless_trajectory_->Append(t3_, *d3_);
  Trajectory<World>* fork = massless_trajectory_->Fork(t2_);
  fork->Append(t4_, *d4_);

  EXPECT_FALSE(massless_trajectory_->has_intrinsic_acceleration());
  massless_trajectory_->set_intrinsic_acceleration(
      [this](Instant const& t) {
        return Vector<Acceleration, World>(
            {1 * SIUnit<Length>() / ((t - t0_) * (t - t0_)),
             2 * SIUnit<Length>() / ((t - t0_) * (t - t0_)),
             3 * SIUnit<Length>() / ((t - t0_) * (t - t0_))});});
  EXPECT_TRUE(massless_trajectory_->has_intrinsic_acceleration());
  EXPECT_THAT(massless_trajectory_->evaluate_intrinsic_acceleration(t1_),
              Eq(Vector<Acceleration, World>(
                  {0.020408163265306122449 * SIUnit<Acceleration>(),
                   0.040816326530612244898 * SIUnit<Acceleration>(),
                   0.061224489795918367347 * SIUnit<Acceleration>()})));

  EXPECT_FALSE(fork->has_intrinsic_acceleration());
  fork->set_intrinsic_acceleration(
      [this](Instant const& t) {
        return Vector<Acceleration, World>(
            {4 * SIUnit<Length>() / ((t - t0_) * (t - t0_)),
             5 * SIUnit<Length>() / ((t - t0_) * (t - t0_)),
             6 * SIUnit<Length>() / ((t - t0_) * (t - t0_))});});
  EXPECT_TRUE(fork->has_intrinsic_acceleration());
  EXPECT_THAT(fork->evaluate_intrinsic_acceleration(t1_),
              Eq(Vector<Acceleration, World>({0 * SIUnit<Acceleration>(),
                                              0 * SIUnit<Acceleration>(),
                                              0 * SIUnit<Acceleration>()})));
  EXPECT_THAT(fork->evaluate_intrinsic_acceleration(t2_),
              Eq(Vector<Acceleration, World>({0 * SIUnit<Acceleration>(),
                                              0 * SIUnit<Acceleration>(),
                                              0 * SIUnit<Acceleration>()})));
  EXPECT_THAT(fork->evaluate_intrinsic_acceleration(t3_),
              Eq(Vector<Acceleration, World>(
                  {0.0054869684499314128944 * SIUnit<Acceleration>(),
                   0.0068587105624142661180 * SIUnit<Acceleration>(),
                   0.0082304526748971193416 * SIUnit<Acceleration>()})));
  fork->clear_intrinsic_acceleration();
  EXPECT_FALSE(fork->has_intrinsic_acceleration());
  EXPECT_TRUE(massless_trajectory_->has_intrinsic_acceleration());

  massless_trajectory_->clear_intrinsic_acceleration();
  EXPECT_FALSE(fork->has_intrinsic_acceleration());
  EXPECT_FALSE(massless_trajectory_->has_intrinsic_acceleration());
}

}  // namespace physics
}  // namespace principia
