﻿
#include "physics/body_surface_dynamic_frame.hpp"

#include <memory>

#include "astronomy/frames.hpp"
#include "geometry/frame.hpp"
#include "geometry/grassmann.hpp"
#include "geometry/named_quantities.hpp"
#include "geometry/rotation.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "integrators/symplectic_runge_kutta_nyström_integrator.hpp"
#include "physics/ephemeris.hpp"
#include "physics/mock_continuous_trajectory.hpp"
#include "physics/mock_ephemeris.hpp"
#include "physics/solar_system.hpp"
#include "quantities/constants.hpp"
#include "quantities/quantities.hpp"
#include "quantities/si.hpp"
#include "serialization/geometry.pb.h"
#include "serialization/physics.pb.h"
#include "testing_utilities/almost_equals.hpp"
#include "testing_utilities/numerics.hpp"

namespace principia {
namespace physics {
namespace internal_body_surface_dynamic_frame {

using astronomy::ICRFJ2000Equator;
using base::check_not_null;
using geometry::Bivector;
using geometry::Displacement;
using geometry::Instant;
using geometry::Rotation;
using geometry::Velocity;
using geometry::Vector;
using quantities::GravitationalParameter;
using quantities::Pow;
using quantities::Time;
using quantities::si::Kilo;
using quantities::si::Metre;
using quantities::si::Milli;
using quantities::si::Radian;
using quantities::si::Second;
using testing_utilities::AbsoluteError;
using testing_utilities::AlmostEquals;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::IsNull;
using ::testing::Lt;
using ::testing::Not;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

namespace {

char constexpr big[] = "Big";
char constexpr small[] = "Small";

}  // namespace

class BodySurfaceDynamicFrameTest : public ::testing::Test {
 protected:
  // The rotating frame centred on the big body and directed to the small one.
  using BigSmallFrame = Frame<serialization::Frame::TestTag,
                              serialization::Frame::TEST, /*inertial=*/false>;
  using MockFrame = Frame<serialization::Frame::TestTag,
                          serialization::Frame::TEST1, /*inertial=*/false>;

  BodySurfaceDynamicFrameTest()
      : period_(10 * π * sqrt(5.0 / 7.0) * Second),
        big_initial_state_(Position<ICRFJ2000Equator>(),
                           Velocity<ICRFJ2000Equator>()),
        small_initial_state_(Position<ICRFJ2000Equator>(),
                             Velocity<ICRFJ2000Equator>()) {
    solar_system_.Initialize(
        SOLUTION_DIR / "astronomy" / "gravity_model_two_bodies_test.proto.txt",
        SOLUTION_DIR / "astronomy" /
            "initial_state_two_bodies_circular_test.proto.txt");
    t0_ = solar_system_.epoch();
    ephemeris_ = solar_system_.MakeEphemeris(
                    /*fitting_tolerance=*/1 * Milli(Metre),
                    Ephemeris<ICRFJ2000Equator>::FixedStepParameters(
                        integrators::McLachlanAtela1992Order4Optimal<
                            Position<ICRFJ2000Equator>>(),
                        /*step=*/10 * Milli(Second)));
    big_ = solar_system_.massive_body(*ephemeris_, big);
    small_ = solar_system_.massive_body(*ephemeris_, small);
    ephemeris_->Prolong(t0_ + 2 * period_);
    big_initial_state_ = solar_system_.initial_state(big);
    big_gravitational_parameter_ = solar_system_.gravitational_parameter(big);
    small_initial_state_ = solar_system_.initial_state(small);
    small_gravitational_parameter_ =
        solar_system_.gravitational_parameter(small);
    big_frame_ = std::make_unique<
        BodySurfaceDynamicFrame<ICRFJ2000Equator, BigSmallFrame>>(
        ephemeris_.get(), big_);

    mock_ephemeris_ =
       std::make_unique<StrictMock<MockEphemeris<ICRFJ2000Equator>>>();
    EXPECT_CALL(*mock_ephemeris_,
                trajectory(solar_system_.massive_body(*ephemeris_, big)))
        .WillOnce(Return(&mock_big_trajectory_));
    EXPECT_CALL(*mock_ephemeris_,
                trajectory(solar_system_.massive_body(*ephemeris_, small)))
        .WillOnce(Return(&mock_small_trajectory_));
    mock_frame_ = std::make_unique<StrictMock<
        BodySurfaceDynamicFrame<ICRFJ2000Equator, MockFrame>>>(
        mock_ephemeris_.get(), big_);
  }

  Time const period_;
  Instant t0_;
  MassiveBody const* big_;
  MassiveBody const* small_;
  DegreesOfFreedom<ICRFJ2000Equator> big_initial_state_;
  DegreesOfFreedom<ICRFJ2000Equator> small_initial_state_;
  GravitationalParameter big_gravitational_parameter_;
  GravitationalParameter small_gravitational_parameter_;
  std::unique_ptr<
      BodySurfaceDynamicFrame<ICRFJ2000Equator, BigSmallFrame>> big_frame_;
  std::unique_ptr<Ephemeris<ICRFJ2000Equator>> ephemeris_;
  SolarSystem<ICRFJ2000Equator> solar_system_;

  StrictMock<MockContinuousTrajectory<ICRFJ2000Equator>> mock_big_trajectory_;
  StrictMock<MockContinuousTrajectory<ICRFJ2000Equator>> mock_small_trajectory_;
  std::unique_ptr<StrictMock<
      BodySurfaceDynamicFrame<ICRFJ2000Equator, MockFrame>>> mock_frame_;
  std::unique_ptr<StrictMock<MockEphemeris<ICRFJ2000Equator>>> mock_ephemeris_;
};


TEST_F(BodySurfaceDynamicFrameTest, ToBigSmallFrameAtTime) {
  int const steps = 100;

  ContinuousTrajectory<ICRFJ2000Equator>::Hint big_hint;
  ContinuousTrajectory<ICRFJ2000Equator>::Hint small_hint;
  for (Instant t = t0_; t < t0_ + 1 * period_; t += period_ / steps) {
    auto const to_big_frame_at_t = big_frame_->ToThisFrameAtTime(t);

    // Check that the bodies don't move and are at the right locations.
    DegreesOfFreedom<ICRFJ2000Equator> const big_in_inertial_frame_at_t =
        solar_system_.trajectory(*ephemeris_, big).
            EvaluateDegreesOfFreedom(t, &big_hint);
    DegreesOfFreedom<ICRFJ2000Equator> const small_in_inertial_frame_at_t =
        solar_system_.trajectory(*ephemeris_, small).
            EvaluateDegreesOfFreedom(t, &small_hint);

    DegreesOfFreedom<BigSmallFrame> const big_in_big_small_at_t =
        to_big_frame_at_t(big_in_inertial_frame_at_t);
    DegreesOfFreedom<BigSmallFrame> const small_in_big_small_at_t =
        to_big_frame_at_t(small_in_inertial_frame_at_t);
    EXPECT_THAT(AbsoluteError(big_in_big_small_at_t.position(),
                              BigSmallFrame::origin),
                Lt(1.0e-6 * Metre));
    EXPECT_THAT(AbsoluteError(big_in_big_small_at_t.velocity(),
                              Velocity<BigSmallFrame>()),
                Lt(1.0e-4 * Metre / Second));
    EXPECT_THAT(AbsoluteError(small_in_big_small_at_t.position(),
                              Displacement<BigSmallFrame>({
                                  5.0 * Kilo(Metre),
                                  0 * Kilo(Metre),
                                  0 * Kilo(Metre)}) + BigSmallFrame::origin),
                Lt(1.0e-5 * Metre));
    EXPECT_THAT(AbsoluteError(small_in_big_small_at_t.velocity(),
                              Velocity<BigSmallFrame>()),
                Lt(1.0e-4 * Metre / Second));
  }
}

TEST_F(BodySurfaceDynamicFrameTest, Inverse) {
  int const steps = 100;
  for (Instant t = t0_; t < t0_ + 1 * period_; t += period_ / steps) {
    auto const from_big_frame_at_t =
        big_frame_->FromThisFrameAtTime(t);
    auto const to_big_frame_at_t = big_frame_->ToThisFrameAtTime(t);
    auto const small_initial_state_transformed_and_back =
        from_big_frame_at_t(to_big_frame_at_t(
            small_initial_state_));
    EXPECT_THAT(
        AbsoluteError(small_initial_state_transformed_and_back.position(),
                      small_initial_state_.position()),
        Lt(1.0e-11 * Metre));
    EXPECT_THAT(
        AbsoluteError(small_initial_state_transformed_and_back.velocity(),
                      small_initial_state_.velocity()),
        Lt(1.0e-11 * Metre / Second));
  }
}

// The test point is at the origin and in motion.  The acceleration is purely
// due to Coriolis.
TEST_F(BodySurfaceDynamicFrameTest, CoriolisAcceleration) {
  Instant const t = t0_ + 0 * Second;
  // The velocity is opposed to the motion and away from the centre.
  DegreesOfFreedom<MockFrame> const point_dof =
      {Displacement<MockFrame>({0 * Metre, 0 * Metre, 0 * Metre}) +
           MockFrame::origin,
       Velocity<MockFrame>({10 * Metre / Second,
                            20 * Metre / Second,
                            30 * Metre / Second})};
  DegreesOfFreedom<ICRFJ2000Equator> const big_dof =
      {Displacement<ICRFJ2000Equator>({0 * Metre, 0 * Metre, 0 * Metre}) +
           ICRFJ2000Equator::origin,
       Velocity<ICRFJ2000Equator>()};
  DegreesOfFreedom<ICRFJ2000Equator> const small_dof =
      {Displacement<ICRFJ2000Equator>({3 * Metre, 4 * Metre, 0 * Metre}) +
           ICRFJ2000Equator::origin,
       Velocity<ICRFJ2000Equator>({40 * Metre / Second,
                                   -30 * Metre / Second,
                                   0 * Metre / Second})};

  EXPECT_CALL(mock_big_trajectory_, EvaluateDegreesOfFreedom(t, _))
      .Times(2)
      .WillRepeatedly(Return(big_dof));
  EXPECT_CALL(mock_small_trajectory_, EvaluateDegreesOfFreedom(t, _))
      .Times(2)
      .WillRepeatedly(Return(small_dof));
  {
    InSequence s;
    EXPECT_CALL(*mock_ephemeris_,
                ComputeGravitationalAccelerationOnMassiveBody(
                    check_not_null(big_), t))
        .WillOnce(Return(Vector<Acceleration, ICRFJ2000Equator>({
                             0 * Metre / Pow<2>(Second),
                             0 * Metre / Pow<2>(Second),
                             0 * Metre / Pow<2>(Second)})));
    EXPECT_CALL(*mock_ephemeris_,
                ComputeGravitationalAccelerationOnMassiveBody(
                    check_not_null(small_), t))
        .WillOnce(Return(Vector<Acceleration, ICRFJ2000Equator>({
                             -300 * Metre / Pow<2>(Second),
                             -400 * Metre / Pow<2>(Second),
                             0 * Metre / Pow<2>(Second)})));
    EXPECT_CALL(*mock_ephemeris_,
                ComputeGravitationalAccelerationOnMasslessBody(_, t))
        .WillOnce(Return(Vector<Acceleration, ICRFJ2000Equator>()));
  }

  // The Coriolis acceleration is towards the centre and opposed to the motion.
  EXPECT_THAT(mock_frame_->GeometricAcceleration(t, point_dof),
              AlmostEquals(Vector<Acceleration, MockFrame>({
                               400 * Metre / Pow<2>(Second),
                               -200 * Metre / Pow<2>(Second),
                               0 * Metre / Pow<2>(Second)}), 0));
}

// The test point doesn't move so the acceleration is purely centrifugal.
TEST_F(BodySurfaceDynamicFrameTest, CentrifugalAcceleration) {
  Instant const t = t0_ + 0 * Second;
  DegreesOfFreedom<MockFrame> const point_dof =
      {Displacement<MockFrame>({10 * Metre, 20 * Metre, 30 * Metre}) +
           MockFrame::origin,
       Velocity<MockFrame>({0 * Metre / Second,
                            0 * Metre / Second,
                            0 * Metre / Second})};
  DegreesOfFreedom<ICRFJ2000Equator> const big_dof =
      {Displacement<ICRFJ2000Equator>({0 * Metre, 0 * Metre, 0 * Metre}) +
           ICRFJ2000Equator::origin,
       Velocity<ICRFJ2000Equator>({0 * Metre / Second,
                                   0 * Metre / Second,
                                   0 * Metre / Second})};
  DegreesOfFreedom<ICRFJ2000Equator> const small_dof =
      {Displacement<ICRFJ2000Equator>({3 * Metre, 4 * Metre, 0 * Metre}) +
           ICRFJ2000Equator::origin,
       Velocity<ICRFJ2000Equator>({40 * Metre / Second,
                                   -30 * Metre / Second,
                                   0 * Metre / Second})};

  EXPECT_CALL(mock_big_trajectory_, EvaluateDegreesOfFreedom(t, _))
      .Times(2)
      .WillRepeatedly(Return(big_dof));
  EXPECT_CALL(mock_small_trajectory_, EvaluateDegreesOfFreedom(t, _))
      .Times(2)
      .WillRepeatedly(Return(small_dof));
  {
    InSequence s;
    EXPECT_CALL(*mock_ephemeris_,
                ComputeGravitationalAccelerationOnMassiveBody(
                    check_not_null(big_), t))
        .WillOnce(Return(Vector<Acceleration, ICRFJ2000Equator>({
                             0 * Metre / Pow<2>(Second),
                             0 * Metre / Pow<2>(Second),
                             0 * Metre / Pow<2>(Second)})));
    EXPECT_CALL(*mock_ephemeris_,
                ComputeGravitationalAccelerationOnMassiveBody(
                    check_not_null(small_), t))
        .WillOnce(Return(Vector<Acceleration, ICRFJ2000Equator>({
                             -300 * Metre / Pow<2>(Second),
                             -400 * Metre / Pow<2>(Second),
                             0 * Metre / Pow<2>(Second)})));
    EXPECT_CALL(*mock_ephemeris_,
                ComputeGravitationalAccelerationOnMasslessBody(_, t))
        .WillOnce(Return(Vector<Acceleration, ICRFJ2000Equator>()));
  }

  EXPECT_THAT(mock_frame_->GeometricAcceleration(t, point_dof),
              AlmostEquals(Vector<Acceleration, MockFrame>({
                               1e3 * Metre / Pow<2>(Second),
                               2e3 * Metre / Pow<2>(Second),
                               0 * Metre / Pow<2>(Second)}), 0));
}

// A tangential acceleration that increases the rotational speed.  The test
// point doesn't move.  The resulting acceleration combines centrifugal and
// Euler.
TEST_F(BodySurfaceDynamicFrameTest, EulerAcceleration) {
  Instant const t = t0_ + 0 * Second;
  DegreesOfFreedom<MockFrame> const point_dof =
      {Displacement<MockFrame>({10 * Metre, 20 * Metre, 30 * Metre}) +
           MockFrame::origin,
       Velocity<MockFrame>({0 * Metre / Second,
                            0 * Metre / Second,
                            0 * Metre / Second})};
  DegreesOfFreedom<ICRFJ2000Equator> const big_dof =
      {Displacement<ICRFJ2000Equator>({0 * Metre, 0 * Metre, 0 * Metre}) +
           ICRFJ2000Equator::origin,
       Velocity<ICRFJ2000Equator>({0 * Metre / Second,
                                   0 * Metre / Second,
                                   0 * Metre / Second})};
  DegreesOfFreedom<ICRFJ2000Equator> const small_dof =
      {Displacement<ICRFJ2000Equator>({3 * Metre, 4 * Metre, 0 * Metre}) +
           ICRFJ2000Equator::origin,
       Velocity<ICRFJ2000Equator>({40 * Metre / Second,
                                   -30 * Metre / Second,
                                   0 * Metre / Second})};
  EXPECT_CALL(mock_big_trajectory_, EvaluateDegreesOfFreedom(t, _))
      .Times(2)
      .WillRepeatedly(Return(big_dof));
  EXPECT_CALL(mock_small_trajectory_, EvaluateDegreesOfFreedom(t, _))
      .Times(2)
      .WillRepeatedly(Return(small_dof));
  {
    // The acceleration is centripetal + tangential.
    InSequence s;
    EXPECT_CALL(*mock_ephemeris_,
                ComputeGravitationalAccelerationOnMassiveBody(
                    check_not_null(big_), t))
        .WillOnce(Return(Vector<Acceleration, ICRFJ2000Equator>({
                             0 * Metre / Pow<2>(Second),
                             0 * Metre / Pow<2>(Second),
                             0 * Metre / Pow<2>(Second)})));
    EXPECT_CALL(*mock_ephemeris_,
                ComputeGravitationalAccelerationOnMassiveBody(
                    check_not_null(small_), t))
        .WillOnce(Return(Vector<Acceleration, ICRFJ2000Equator>({
                             (-300 + 400) * Metre / Pow<2>(Second),
                             (-400 - 300) * Metre / Pow<2>(Second),
                             0 * Metre / Pow<2>(Second)})));
    EXPECT_CALL(*mock_ephemeris_,
                ComputeGravitationalAccelerationOnMasslessBody(_, t))
        .WillOnce(Return(Vector<Acceleration, ICRFJ2000Equator>()));
  }

  // The acceleration is centrifugal + Euler.
  EXPECT_THAT(mock_frame_->GeometricAcceleration(t, point_dof),
              AlmostEquals(Vector<Acceleration, MockFrame>({
                               (1e3 + 2e3) * Metre / Pow<2>(Second),
                               (2e3 - 1e3) * Metre / Pow<2>(Second),
                               0 * Metre / Pow<2>(Second)}), 0));
}

// A linear acceleration identical for both bodies.  The test point doesn't
// move.  The resulting acceleration combines centrifugal and linear.
TEST_F(BodySurfaceDynamicFrameTest, LinearAcceleration) {
  Instant const t = t0_ + 0 * Second;
  DegreesOfFreedom<MockFrame> const point_dof =
      {Displacement<MockFrame>({10 * Metre, 20 * Metre, 30 * Metre}) +
           MockFrame::origin,
       Velocity<MockFrame>({0 * Metre / Second,
                            0 * Metre / Second,
                            0 * Metre / Second})};
  DegreesOfFreedom<ICRFJ2000Equator> const big_dof =
      {Displacement<ICRFJ2000Equator>({0 * Metre, 0 * Metre, 0 * Metre}) +
           ICRFJ2000Equator::origin,
       Velocity<ICRFJ2000Equator>({0 * Metre / Second,
                                   0 * Metre / Second,
                                   0 * Metre / Second})};
  DegreesOfFreedom<ICRFJ2000Equator> const small_dof =
      {Displacement<ICRFJ2000Equator>({3 * Metre, 4 * Metre, 0 * Metre}) +
           ICRFJ2000Equator::origin,
       Velocity<ICRFJ2000Equator>({40 * Metre / Second,
                                   -30 * Metre / Second,
                                   0 * Metre / Second})};

  EXPECT_CALL(mock_big_trajectory_, EvaluateDegreesOfFreedom(t, _))
      .Times(2)
      .WillRepeatedly(Return(big_dof));
  EXPECT_CALL(mock_small_trajectory_, EvaluateDegreesOfFreedom(t, _))
      .Times(2)
      .WillRepeatedly(Return(small_dof));
  {
    // The acceleration is linear + centripetal.
    InSequence s;
    EXPECT_CALL(*mock_ephemeris_,
                ComputeGravitationalAccelerationOnMassiveBody(
                    check_not_null(big_), t))
        .WillOnce(Return(Vector<Acceleration, ICRFJ2000Equator>({
                             -160 * Metre / Pow<2>(Second),
                             120 * Metre / Pow<2>(Second),
                             300 * Metre / Pow<2>(Second)})));
    EXPECT_CALL(*mock_ephemeris_,
                ComputeGravitationalAccelerationOnMassiveBody(
                    check_not_null(small_), t))
        .WillOnce(Return(Vector<Acceleration, ICRFJ2000Equator>({
                             (-160 - 300) * Metre / Pow<2>(Second),
                             (120 - 400) * Metre / Pow<2>(Second),
                             300 * Metre / Pow<2>(Second)})));
    EXPECT_CALL(*mock_ephemeris_,
                ComputeGravitationalAccelerationOnMasslessBody(_, t))
        .WillOnce(Return(Vector<Acceleration, ICRFJ2000Equator>()));
  }

  // The acceleration is linear + centrifugal.
  EXPECT_THAT(mock_frame_->GeometricAcceleration(t, point_dof),
              AlmostEquals(Vector<Acceleration, MockFrame>({
                               1e3 * Metre / Pow<2>(Second),
                               (200 + 2e3) * Metre / Pow<2>(Second),
                               300 * Metre / Pow<2>(Second)}), 0));
}

TEST_F(BodySurfaceDynamicFrameTest, GeometricAcceleration) {
  Instant const t = t0_ + period_;
  DegreesOfFreedom<BigSmallFrame> const point_dof =
      {Displacement<BigSmallFrame>({10 * Metre, 20 * Metre, 30 * Metre}) +
           BigSmallFrame::origin,
       Velocity<BigSmallFrame>({3 * Metre / Second,
                                2 * Metre / Second,
                                1 * Metre / Second})};
  // We trust the functions to compute the values correctly, but this test
  // ensures that we don't get NaNs.
  EXPECT_THAT(big_frame_->GeometricAcceleration(t, point_dof),
              AlmostEquals(Vector<Acceleration, BigSmallFrame>({
                  -9.54502614154907060e5 * Metre / Pow<2>(Second),
                  -1.90900949256416666e6 * Metre / Pow<2>(Second),
                  -2.86351378905829135e6 * Metre / Pow<2>(Second)}), 0));
}

TEST_F(BodySurfaceDynamicFrameTest, Serialization) {
  serialization::DynamicFrame message;
  big_frame_->WriteToMessage(&message);

  EXPECT_TRUE(message.HasExtension(
      serialization::BodySurfaceDynamicFrame::extension));
  auto const extension = message.GetExtension(
      serialization::BodySurfaceDynamicFrame::extension);
  EXPECT_TRUE(extension.has_centre());
  EXPECT_EQ(0, extension.centre());

  auto const read_big_frame =
      DynamicFrame<ICRFJ2000Equator, BigSmallFrame>::ReadFromMessage(
          ephemeris_.get(), message);
  EXPECT_THAT(read_big_frame, Not(IsNull()));

  Instant const t = t0_ + period_;
  DegreesOfFreedom<BigSmallFrame> const point_dof =
      {Displacement<BigSmallFrame>({10 * Metre, 20 * Metre, 30 * Metre}) +
           BigSmallFrame::origin,
       Velocity<BigSmallFrame>({3 * Metre / Second,
                                2 * Metre / Second,
                                1 * Metre / Second})};
  EXPECT_EQ(big_frame_->GeometricAcceleration(t, point_dof),
            read_big_frame->GeometricAcceleration(t, point_dof));
}

}  // namespace internal_body_surface_dynamic_frame
}  // namespace physics
}  // namespace principia
