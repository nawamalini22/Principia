﻿
#include "ksp_plugin/pile_up.hpp"
#include "ksp_plugin/vessel.hpp"

#include <list>

namespace principia {
namespace ksp_plugin {
namespace internal_pile_up {

using base::FindOrDie;
using geometry::AngularVelocity;
using geometry::BarycentreCalculator;
using geometry::OrthogonalMap;
using geometry::Position;
using physics::DegreesOfFreedom;
using physics::RigidMotion;
using physics::RigidTransformation;

PileUp::PileUp(std::list<not_null<Vessel*>>&& vessels)
    : vessels_(std::move(vessels)) {
  BarycentreCalculator<DegreesOfFreedom<Barycentric>, Mass> barycentre;
  for (not_null<Vessel*> vessel : vessels_) {
    CHECK(vessel->last_point_psychohistory_point_is_authoritative());
    barycentre.Add(vessel->psychohistory().last().degrees_of_freedom(),
                   vessel->mass());
  }
  history_.Append(vessels_.front()->psychohistory().last().time(),
                  barycentre.Get());
  last_point_is_authoritative_ = true;
}

void PileUp::set_mass_and_intrinsic_force(
    Mass const& mass,
    Vector<Force, Barycentric> const& intrinsic_force) {
  mass_ = mass;
  intrinsic_force_ = intrinsic_force;
}

std::list<not_null<Vessel*>> const& PileUp::vessels() const {
  return vessels_;
}

void PileUp::AdvanceTime(
    Ephemeris<Barycentric>& ephemeris,
    Instant const& t,
    Ephemeris<Barycentric>::FixedStepParameters const& fixed_step_parameters,
    Ephemeris<Barycentric>::AdaptiveStepParameters const&
        adaptive_step_parameters) {
  if (!last_point_is_authoritative_) {
    auto const penultimate = --history_.last();
    history_.ForgetAfter(penultimate.time());
    last_point_is_authoritative_ = true;
  }
  auto const last_preexisting_authoritative_point_ = history_.last();

  if (intrinsic_force_ == Vector<Force, Barycentric>{}) {
    ephemeris.FlowWithFixedStep(
        {&history_},
        Ephemeris<Barycentric>::NoIntrinsicAccelerations,
        t,
        fixed_step_parameters);
    if (history_.last().time() < t) {
      // TODO(egg): this is clumsy, we need an option for FlowWithAdaptiveStep
      // to only add the last point.  Amusingly, this is the unwanted behaviour
      // of FlowWithFixedStep (#228).
      DiscreteTrajectory<Barycentric> prolongation;
      prolongation.Append(history_.last().time(),
                          history_.last().degrees_of_freedom());
      ephemeris.FlowWithAdaptiveStep(
          &history_,
          Ephemeris<Barycentric>::NoIntrinsicAcceleration,
          t,
          adaptive_step_parameters,
          Ephemeris<Barycentric>::unlimited_max_ephemeris_steps);
      history_.Append(prolongation.last().time(),
                      prolongation.last().degrees_of_freedom());
      last_point_is_authoritative_ = false;
    }
  } else {
    auto const a = intrinsic_force_ / mass_;
    auto const intrinsic_acceleration = [a](Instant const& t) { return a; };
    ephemeris.FlowWithAdaptiveStep(
        &history_,
        intrinsic_acceleration,
        t,
        adaptive_step_parameters,
        Ephemeris<Barycentric>::unlimited_max_ephemeris_steps);
  }
  auto it = last_preexisting_authoritative_point_;
  ++it;
  for (; it != history_.End(); ++it) {
    auto const& pile_up_dof = it.degrees_of_freedom();
    RigidMotion<Barycentric, RigidPileUp> const from_barycentric(
        RigidTransformation<Barycentric, RigidPileUp>(
            pile_up_dof.position(),
            RigidPileUp::origin,
            OrthogonalMap<Barycentric, RigidPileUp>::Identity()),
        AngularVelocity<Barycentric>{},
        pile_up_dof.velocity());
    auto const to_barycentric = from_barycentric.Inverse();
    bool const authoritative =
        last_point_is_authoritative_ || it != history_.last();
    for (not_null<Vessel*> const vessel : vessels_)  {
      vessel->AppendToPsychohistory(
          it.time(),
          to_barycentric(FindOrDie(vessel_degrees_of_freedom_, vessel)),
          authoritative);
    }
  }
}

}  // namespace internal_pile_up
}  // namespace ksp_plugin
}  // namespace principia
