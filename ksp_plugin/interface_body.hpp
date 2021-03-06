﻿
#pragma once

#include "ksp_plugin/interface.hpp"

#include <cmath>
#include <limits>

namespace principia {
namespace interface {

using integrators::AdaptiveStepSizeIntegrator;
using physics::Ephemeris;
using quantities::si::Degree;
using quantities::si::Metre;
using quantities::si::Radian;
using quantities::si::Second;

// No partial specialization of functions, so we wrap everything into structs.
// C++, I hate you.

template<typename T>
struct QPConverter {};

template<typename Frame>
struct QPConverter<DegreesOfFreedom<Frame>> {
  static DegreesOfFreedom<Frame> FromQP(QP const& qp) {
    return DegreesOfFreedom<Frame>(
        XYZConverter<Position<Frame>>::FromXYZ(qp.q),
        XYZConverter<Velocity<Frame>>::FromXYZ(qp.p));
  }
  static QP ToQP(DegreesOfFreedom<Frame> const& dof) {
    return {XYZConverter<Position<Frame>>::ToXYZ(dof.position()),
            XYZConverter<Velocity<Frame>>::ToXYZ(dof.velocity())};
  }
};

template<typename Frame>
struct QPConverter<RelativeDegreesOfFreedom<Frame>> {
  static RelativeDegreesOfFreedom<Frame> FromQP(QP const& qp) {
    return RelativeDegreesOfFreedom<Frame>(
        XYZConverter<Displacement<Frame>>::FromXYZ(qp.q),
        XYZConverter<Velocity<Frame>>::FromXYZ(qp.p));
  }
  static QP ToQP(RelativeDegreesOfFreedom<Frame> const& relative_dof) {
    return {
        XYZConverter<Displacement<Frame>>::ToXYZ(relative_dof.displacement()),
        XYZConverter<Velocity<Frame>>::ToXYZ(relative_dof.velocity())};
  }
};

template<typename T>
struct XYZConverter {};

template<typename Frame>
struct XYZConverter<Displacement<Frame>> {
  static Displacement<Frame> FromXYZ(XYZ const& xyz) {
    return Displacement<Frame>(interface::FromXYZ(xyz) * Metre);
  }
  static XYZ ToXYZ(Displacement<Frame> const& displacement) {
    return interface::ToXYZ(displacement.coordinates() / Metre);
  }
};

template<typename Frame>
struct XYZConverter<Position<Frame>> {
  static Position<Frame> FromXYZ(XYZ const& xyz) {
    return Position<Frame>(Frame::origin +
                           XYZConverter<Displacement<Frame>>::FromXYZ(xyz));
  }
  static XYZ ToXYZ(Position<Frame> const& position) {
    return XYZConverter<Displacement<Frame>>::ToXYZ(position - Frame::origin);
  }
};

template<typename Frame>
struct XYZConverter<Velocity<Frame>> {
  static Velocity<Frame> FromXYZ(XYZ const& xyz) {
    return Velocity<Frame>(interface::FromXYZ(xyz) * (Metre / Second));
  }
  static XYZ ToXYZ(Velocity<Frame> const& velocity) {
    return interface::ToXYZ(velocity.coordinates() / (Metre / Second));
  }
};

inline bool NaNIndependentEq(double const left, double const right) {
  return (left == right) || (std::isnan(left) && std::isnan(right));
}

template<typename Container>
TypedIterator<Container>::TypedIterator(Container container)
    : container_(std::move(container)),
      iterator_(container_.begin()) {}

template<typename Container>
template<typename Interchange>
Interchange TypedIterator<Container>::Get(
    std::function<Interchange(typename Container::value_type const&)> const&
        convert) const {
  CHECK(iterator_ != container_.end());
  return convert(*iterator_);
}

template<typename Container>
bool TypedIterator<Container>::AtEnd() const {
  return iterator_ == container_.end();
}

template<typename Container>
void TypedIterator<Container>::Increment() {
  ++iterator_;
}

template<typename Container>
int TypedIterator<Container>::Size() const {
  return container_.size();
}

inline TypedIterator<DiscreteTrajectory<World>>::TypedIterator(
    not_null<std::unique_ptr<DiscreteTrajectory<World>>> trajectory,
    not_null<Plugin const*> const plugin)
    : trajectory_(std::move(trajectory)),
      iterator_(trajectory_->Begin()),
      plugin_(plugin) {
  CHECK(trajectory_->is_root());
}

template<typename Interchange>
Interchange TypedIterator<DiscreteTrajectory<World>>::Get(
    std::function<Interchange(
        DiscreteTrajectory<World>::Iterator const&)> const& convert) const {
  CHECK(iterator_ != trajectory_->End());
  return convert(iterator_);
}

inline bool TypedIterator<DiscreteTrajectory<World>>::AtEnd() const {
  return iterator_ == trajectory_->End();
}

inline void TypedIterator<DiscreteTrajectory<World>>::Increment() {
  ++iterator_;
}

inline int TypedIterator<DiscreteTrajectory<World>>::Size() const {
  return trajectory_->Size();
}

inline not_null<Plugin const*> TypedIterator<
    DiscreteTrajectory<World>>::plugin() const {
  return plugin_;
}


template<typename T>
std::unique_ptr<T> TakeOwnership(T** const pointer) {
  CHECK_NOTNULL(pointer);
  std::unique_ptr<T> owned_pointer(*pointer);
  *pointer = nullptr;
  return owned_pointer;
}

template<typename T>
std::unique_ptr<T[]> TakeOwnershipArray(T** const pointer) {
  CHECK_NOTNULL(pointer);
  std::unique_ptr<T[]> owned_pointer(*pointer);
  *pointer = nullptr;
  return owned_pointer;
}

inline bool operator==(AdaptiveStepParameters const& left,
                       AdaptiveStepParameters const& right) {
  return left.integrator_kind == right.integrator_kind &&
         left.max_steps == right.max_steps &&
         NaNIndependentEq(left.length_integration_tolerance,
                          right.length_integration_tolerance) &&
         NaNIndependentEq(left.speed_integration_tolerance,
                          right.speed_integration_tolerance);
}

inline bool operator==(Burn const& left, Burn const& right) {
  return NaNIndependentEq(left.thrust_in_kilonewtons,
                          right.thrust_in_kilonewtons) &&
         NaNIndependentEq(left.specific_impulse_in_seconds_g0,
                          right.specific_impulse_in_seconds_g0) &&
         left.frame == right.frame &&
         NaNIndependentEq(left.initial_time, right.initial_time) &&
         left.delta_v == right.delta_v;
}

inline bool operator==(NavigationFrameParameters const& left,
                       NavigationFrameParameters const& right) {
  return left.extension == right.extension &&
         left.centre_index == right.centre_index &&
         left.primary_index == right.primary_index &&
         left.secondary_index == right.secondary_index;
}

inline bool operator==(NavigationManoeuvre const& left,
                       NavigationManoeuvre const& right) {
  return left.burn == right.burn &&
         NaNIndependentEq(left.initial_mass_in_tonnes,
                          right.initial_mass_in_tonnes) &&
         NaNIndependentEq(left.final_mass_in_tonnes,
                          right.final_mass_in_tonnes) &&
         NaNIndependentEq(left.mass_flow, right.mass_flow) &&
         NaNIndependentEq(left.duration, right.duration) &&
         NaNIndependentEq(left.final_time, right.final_time) &&
         NaNIndependentEq(left.time_of_half_delta_v,
                          right.time_of_half_delta_v) &&
         NaNIndependentEq(left.time_to_half_delta_v,
                          right.time_to_half_delta_v) &&
         left.inertial_direction == right.inertial_direction;
}

inline bool operator==(NavigationManoeuvreFrenetTrihedron const& left,
                       NavigationManoeuvreFrenetTrihedron const& right) {
  return left.binormal == right.binormal &&
         left.normal == right.normal &&
         left.tangent == right.tangent;
}

inline bool operator==(QP const& left, QP const& right) {
  return left.q == right.q && left.p == right.p;
}

inline bool operator==(WXYZ const& left, WXYZ const& right) {
  return NaNIndependentEq(left.w, right.w) &&
         NaNIndependentEq(left.x, right.x) &&
         NaNIndependentEq(left.y, right.y) &&
         NaNIndependentEq(left.z, right.z);
}

inline bool operator==(XYZ const& left, XYZ const& right) {
  return NaNIndependentEq(left.x, right.x) &&
         NaNIndependentEq(left.y, right.y) &&
         NaNIndependentEq(left.z, right.z);
}

inline physics::Ephemeris<Barycentric>::AdaptiveStepParameters
FromAdaptiveStepParameters(
    AdaptiveStepParameters const& adaptive_step_parameters) {
  serialization::AdaptiveStepSizeIntegrator message;
  CHECK(serialization::AdaptiveStepSizeIntegrator::Kind_IsValid(
      adaptive_step_parameters.integrator_kind));
  message.set_kind(static_cast<serialization::AdaptiveStepSizeIntegrator::Kind>(
      adaptive_step_parameters.integrator_kind));
  return Ephemeris<Barycentric>::AdaptiveStepParameters(
      AdaptiveStepSizeIntegrator<Ephemeris<
          Barycentric>::NewtonianMotionEquation>::ReadFromMessage(message),
      adaptive_step_parameters.max_steps,
      adaptive_step_parameters.length_integration_tolerance * Metre,
      adaptive_step_parameters.speed_integration_tolerance * (Metre / Second));
}

inline physics::KeplerianElements<Barycentric> FromKeplerianElements(
    KeplerianElements const& keplerian_elements) {
  physics::KeplerianElements<Barycentric> barycentric_keplerian_elements;
  barycentric_keplerian_elements.eccentricity = keplerian_elements.eccentricity;
  barycentric_keplerian_elements.semimajor_axis =
      std::isnan(keplerian_elements.semimajor_axis)
          ? std::experimental::nullopt
          : std::experimental::make_optional(keplerian_elements.semimajor_axis *
                                             Metre);
  barycentric_keplerian_elements.mean_motion =
      std::isnan(keplerian_elements.mean_motion)
          ? std::experimental::nullopt
          : std::experimental::make_optional(keplerian_elements.mean_motion *
                                             Radian / Second);
  barycentric_keplerian_elements.inclination =
      keplerian_elements.inclination_in_degrees * Degree;
  barycentric_keplerian_elements.longitude_of_ascending_node =
      keplerian_elements.longitude_of_ascending_node_in_degrees * Degree;
  barycentric_keplerian_elements.argument_of_periapsis =
      keplerian_elements.argument_of_periapsis_in_degrees * Degree;
  barycentric_keplerian_elements.mean_anomaly =
      keplerian_elements.mean_anomaly * Radian;
  return barycentric_keplerian_elements;
}

template<>
inline DegreesOfFreedom<World> FromQP(QP const& qp) {
  return QPConverter<DegreesOfFreedom<World>>::FromQP(qp);
}

template<>
inline RelativeDegreesOfFreedom<AliceSun> FromQP(QP const& qp) {
  return QPConverter<RelativeDegreesOfFreedom<AliceSun>>::FromQP(qp);
}

template<>
inline RelativeDegreesOfFreedom<World> FromQP(QP const& qp) {
  return QPConverter<RelativeDegreesOfFreedom<World>>::FromQP(qp);
}

inline R3Element<double> FromXYZ(XYZ const& xyz) {
  return {xyz.x, xyz.y, xyz.z};
}

template<>
inline Position<World> FromXYZ<Position<World>>(XYZ const& xyz) {
  return XYZConverter<Position<World>>::FromXYZ(xyz);
}

template<>
Velocity<Frenet<NavigationFrame>>
inline FromXYZ<Velocity<Frenet<NavigationFrame>>>(XYZ const& xyz) {
  return XYZConverter<Velocity<Frenet<NavigationFrame>>>::FromXYZ(xyz);
}

inline AdaptiveStepParameters ToAdaptiveStepParameters(
    physics::Ephemeris<Barycentric>::AdaptiveStepParameters const&
        adaptive_step_parameters) {
  serialization::AdaptiveStepSizeIntegrator message;
  adaptive_step_parameters.integrator().WriteToMessage(&message);
  return {message.kind(),
          adaptive_step_parameters.max_steps(),
          adaptive_step_parameters.length_integration_tolerance() / Metre,
          adaptive_step_parameters.speed_integration_tolerance() /
              (Metre / Second)};
}

inline KeplerianElements ToKeplerianElements(
    physics::KeplerianElements<Barycentric> const& keplerian_elements) {
  return {keplerian_elements.eccentricity,
          keplerian_elements.semimajor_axis
              ? *keplerian_elements.semimajor_axis / Metre
              : std::numeric_limits<double>::quiet_NaN(),
          keplerian_elements.mean_motion
              ? *keplerian_elements.mean_motion / (Radian / Second)
              : std::numeric_limits<double>::quiet_NaN(),
          keplerian_elements.inclination / Degree,
          keplerian_elements.longitude_of_ascending_node / Degree,
          keplerian_elements.argument_of_periapsis / Degree,
          keplerian_elements.mean_anomaly / Radian};
}

inline QP ToQP(DegreesOfFreedom<World> const& dof) {
  return QPConverter<DegreesOfFreedom<World>>::ToQP(dof);
}

inline QP ToQP(RelativeDegreesOfFreedom<AliceSun> const& relative_dof) {
  return QPConverter<RelativeDegreesOfFreedom<AliceSun>>::ToQP(relative_dof);
}

inline WXYZ ToWXYZ(geometry::Quaternion const& quaternion) {
  return {quaternion.real_part(),
          quaternion.imaginary_part().x,
          quaternion.imaginary_part().y,
          quaternion.imaginary_part().z};
}

inline XYZ ToXYZ(geometry::R3Element<double> const& r3_element) {
  return {r3_element.x, r3_element.y, r3_element.z};
}

inline XYZ ToXYZ(Position<World> const& position) {
  return XYZConverter<Position<World>>::ToXYZ(position);
}

inline XYZ ToXYZ(Velocity<Frenet<NavigationFrame>> const& velocity) {
  return XYZConverter<Velocity<Frenet<NavigationFrame>>>::ToXYZ(velocity);
}

inline XYZ ToXYZ(Vector<double, World> const& direction) {
  return ToXYZ(direction.coordinates());
}

inline XYZ ToXYZ(Velocity<World> const& velocity) {
  return XYZConverter<Velocity<World>>::ToXYZ(velocity);
}

inline Instant FromGameTime(Plugin const& plugin,
                            double const t) {
  return plugin.GameEpoch() + t * Second;
}

inline double ToGameTime(Plugin const& plugin,
                         Instant const& t) {
  return (t - plugin.GameEpoch()) / Second;
}

inline not_null<std::unique_ptr<NavigationFrame>> NewNavigationFrame(
    Plugin const& plugin,
    NavigationFrameParameters const& parameters) {
  switch (parameters.extension) {
    case serialization::BarycentricRotatingDynamicFrame::
        kExtensionFieldNumber:
      return plugin.NewBarycentricRotatingNavigationFrame(
          parameters.primary_index, parameters.secondary_index);
    case serialization::BodyCentredBodyDirectionDynamicFrame::
        kExtensionFieldNumber:
      return plugin.NewBodyCentredBodyDirectionNavigationFrame(
          parameters.primary_index, parameters.secondary_index);
    case serialization::BodyCentredNonRotatingDynamicFrame::
        kExtensionFieldNumber:
      return plugin.NewBodyCentredNonRotatingNavigationFrame(
          parameters.centre_index);
    case serialization::BodySurfaceDynamicFrame::kExtensionFieldNumber:
      return plugin.NewBodySurfaceNavigationFrame(parameters.centre_index);
    default:
      LOG(FATAL) << "Unexpected extension " << parameters.extension;
      base::noreturn();
  }
}

}  // namespace interface
}  // namespace principia
