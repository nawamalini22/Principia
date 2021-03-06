﻿
#pragma once

#include "physics/solar_system.hpp"

#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "astronomy/epoch.hpp"
#include "astronomy/frames.hpp"
#include "geometry/grassmann.hpp"
#include "geometry/named_quantities.hpp"
#include "geometry/r3_element.hpp"
#include "glog/logging.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"
#include "physics/degrees_of_freedom.hpp"
#include "physics/massive_body.hpp"
#include "physics/oblate_body.hpp"
#include "physics/rotating_body.hpp"
#include "quantities/parser.hpp"
#include "quantities/si.hpp"
#include "serialization/astronomy.pb.h"

namespace principia {
namespace physics {
namespace internal_solar_system {

using astronomy::J2000;
using astronomy::JulianDate;
using base::FindOrDie;
using base::make_not_null_unique;
using geometry::Bivector;
using geometry::Frame;
using geometry::Instant;
using geometry::Position;
using geometry::RadiusLatitudeLongitude;
using geometry::Vector;
using geometry::Velocity;
using quantities::Angle;
using quantities::AngularFrequency;
using quantities::Length;
using quantities::ParseQuantity;
using quantities::Speed;
using quantities::si::Radian;
using quantities::si::Second;

template<typename Frame>
void SolarSystem<Frame>::Initialize(
    std::experimental::filesystem::path const& gravity_model_filename,
    std::experimental::filesystem::path const& initial_state_filename) {
  // Parse the files.
  std::ifstream gravity_model_ifstream(gravity_model_filename);
  CHECK(gravity_model_ifstream.good());
  google::protobuf::io::IstreamInputStream gravity_model_zcs(
                                               &gravity_model_ifstream);
  CHECK(google::protobuf::TextFormat::Parse(&gravity_model_zcs,
                                            &gravity_model_));
  CHECK(gravity_model_.has_gravity_model());

  std::ifstream initial_state_ifstream(initial_state_filename);
  CHECK(initial_state_ifstream.good());
  google::protobuf::io::IstreamInputStream initial_state_zcs(
                                               &initial_state_ifstream);
  CHECK(google::protobuf::TextFormat::Parse(&initial_state_zcs,
                                            &initial_state_));
  CHECK(initial_state_.has_initial_state());

  // If a frame is specified in the files it must match the frame of this
  // instance.  Otherwise the frame of the instance is used.  This is convenient
  // for tests.
  if (initial_state_.initial_state().has_frame()) {
    CHECK_EQ(Frame::tag, initial_state_.initial_state().frame());
  }
  if (gravity_model_.gravity_model().has_frame()) {
    CHECK_EQ(Frame::tag, gravity_model_.gravity_model().frame());
  }

  // Store the data in maps keyed by body name.
  for (auto& body : *gravity_model_.mutable_gravity_model()->mutable_body()) {
    auto const inserted =
        gravity_model_map_.insert(std::make_pair(body.name(), &body));
    CHECK(inserted.second);
  }
  for (auto const& body : initial_state_.initial_state().body()) {
    auto const inserted =
        initial_state_map_.insert(std::make_pair(body.name(), &body));
    CHECK(inserted.second);
  }

  // Check that the maps are consistent.
  auto it1 = gravity_model_map_.begin();
  auto it2 = initial_state_map_.begin();
  for (; it1 != gravity_model_map_.end() && it2 != initial_state_map_.end();
       ++it1, ++it2) {
    CHECK_EQ(it1->first, it2->first);
    names_.push_back(it1->first);
  }
  CHECK(it1 == gravity_model_map_.end()) << it1->first;
  CHECK(it2 == initial_state_map_.end()) << it2->first;

  epoch_ = JulianDate(initial_state_.initial_state().epoch());

  // Call these two functions to parse all the data, so that errors are detected
  // at initialization.  Drop their results on the floor.
  MakeAllMassiveBodies();
  MakeAllDegreesOfFreedom();
}

template<typename Frame>
std::unique_ptr<Ephemeris<Frame>> SolarSystem<Frame>::MakeEphemeris(
    Length const& fitting_tolerance,
    typename Ephemeris<Frame>::FixedStepParameters const& parameters) {
  return std::make_unique<Ephemeris<Frame>>(MakeAllMassiveBodies(),
                                            MakeAllDegreesOfFreedom(),
                                            epoch_,
                                            fitting_tolerance,
                                            parameters);
}

template<typename Frame>
Instant const& SolarSystem<Frame>::epoch() const {
  return epoch_;
}

template<typename Frame>
std::vector<std::string> const& SolarSystem<Frame>::names() const {
  return names_;
}

template<typename Frame>
int SolarSystem<Frame>::index(std::string const& name) const {
  auto const it = std::equal_range(names_.begin(), names_.end(), name);
  return it.first - names_.begin();
}


template<typename Frame>
DegreesOfFreedom<Frame> SolarSystem<Frame>::initial_state(
    std::string const& name) const {
  return MakeDegreesOfFreedom(*initial_state_map_.at(name));
}

template<typename Frame>
GravitationalParameter SolarSystem<Frame>::gravitational_parameter(
    std::string const& name) const {
  return MakeMassiveBody(*gravity_model_map_.at(name))->
             gravitational_parameter();
}

template<typename Frame>
Length SolarSystem<Frame>::mean_radius(std::string const& name) const {
  return MakeMassiveBody(*gravity_model_map_.at(name))->mean_radius();
}

template<typename Frame>
not_null<MassiveBody const*> SolarSystem<Frame>::massive_body(
    Ephemeris<Frame> const & ephemeris,
    std::string const & name) const {
  return ephemeris.bodies()[index(name)];
}

template<typename Frame>
ContinuousTrajectory<Frame> const& SolarSystem<Frame>::trajectory(
    Ephemeris<Frame> const & ephemeris,
    std::string const & name) const {
  MassiveBody const* const body = ephemeris.bodies()[index(name)];
  return *ephemeris.trajectory(body);
}

template<typename Frame>
serialization::GravityModel::Body const&
SolarSystem<Frame>::gravity_model_message(std::string const& name) const {
  return *FindOrDie(gravity_model_map_, name);
}

template<typename Frame>
serialization::InitialState::Body const&
SolarSystem<Frame>::initial_state_message(std::string const& name) const {
  return *FindOrDie(initial_state_map_, name);
}

template<typename Frame>
DegreesOfFreedom<Frame> SolarSystem<Frame>::MakeDegreesOfFreedom(
    serialization::InitialState::Body const& body) {
  Position<Frame> const
      position = Frame::origin +
                 Vector<Length, Frame>({ParseQuantity<Length>(body.x()),
                                        ParseQuantity<Length>(body.y()),
                                        ParseQuantity<Length>(body.z())});
  Velocity<Frame> const
      velocity(Vector<Speed, Frame>({ParseQuantity<Speed>(body.vx()),
                                     ParseQuantity<Speed>(body.vy()),
                                     ParseQuantity<Speed>(body.vz())}));
  return DegreesOfFreedom<Frame>(position, velocity);
}

template<typename Frame>
not_null<std::unique_ptr<MassiveBody>> SolarSystem<Frame>::MakeMassiveBody(
    serialization::GravityModel::Body const& body) {
  Check(body);
  auto const massive_body_parameters = MakeMassiveBodyParameters(body);
  if (body.has_mean_radius()) {
    auto const rotating_body_parameters = MakeRotatingBodyParameters(body);
    if (body.has_j2()) {
      auto const oblate_body_parameters = MakeOblateBodyParameters(body);
      return std::make_unique<OblateBody<Frame>>(*massive_body_parameters,
                                                 *rotating_body_parameters,
                                                 *oblate_body_parameters);
    } else {
      return std::make_unique<RotatingBody<Frame>>(*massive_body_parameters,
                                                   *rotating_body_parameters);
    }
  } else {
    return std::make_unique<MassiveBody>(*massive_body_parameters);
  }
}

template<typename Frame>
not_null<std::unique_ptr<RotatingBody<Frame>>>
SolarSystem<Frame>::MakeRotatingBody(
    serialization::GravityModel::Body const& body) {
  Check(body);
  CHECK(body.has_mean_radius());
  auto const massive_body_parameters = MakeMassiveBodyParameters(body);
  auto const rotating_body_parameters = MakeRotatingBodyParameters(body);
  if (body.has_j2()) {
    auto const oblate_body_parameters = MakeOblateBodyParameters(body);
    return std::make_unique<OblateBody<Frame>>(*massive_body_parameters,
                                               *rotating_body_parameters,
                                               *oblate_body_parameters);
  } else {
    return std::make_unique<RotatingBody<Frame>>(*massive_body_parameters,
                                                 *rotating_body_parameters);
  }
}

template<typename Frame>
not_null<std::unique_ptr<OblateBody<Frame>>>
SolarSystem<Frame>::MakeOblateBody(
    serialization::GravityModel::Body const& body) {
  Check(body);
  CHECK(body.has_mean_radius());
  CHECK(body.has_j2());
  auto const massive_body_parameters = MakeMassiveBodyParameters(body);
  auto const rotating_body_parameters = MakeRotatingBodyParameters(body);
  auto const oblate_body_parameters = MakeOblateBodyParameters(body);
  return std::make_unique<OblateBody<Frame>>(*massive_body_parameters,
                                             *rotating_body_parameters,
                                             *oblate_body_parameters);
}

template<typename Frame>
void SolarSystem<Frame>::RemoveMassiveBody(std::string const& name) {
  for (int i = 0; i < names_.size(); ++i) {
    if (names_[i] == name) {
      names_.erase(names_.begin() + i);
      initial_state_map_.erase(name);
      gravity_model_map_.erase(name);
      return;
    }
  }
  LOG(FATAL) << name << " does not exist";
}

template<typename Frame>
void SolarSystem<Frame>::RemoveOblateness(std::string const& name) {
  auto const it = gravity_model_map_.find(name);
  CHECK(it != gravity_model_map_.end()) << name << " does not exist";
  serialization::GravityModel::Body* body = it->second;
  body->clear_j2();
  body->clear_reference_radius();
}

template<typename Frame>
void SolarSystem<Frame>::Check(serialization::GravityModel::Body const& body) {
  CHECK(body.has_name());
  CHECK(body.has_gravitational_parameter()) << body.name();
  CHECK_EQ(body.has_reference_instant(), body.has_mean_radius()) << body.name();
  CHECK_EQ(body.has_reference_instant(),
           body.has_axis_right_ascension()) << body.name();
  CHECK_EQ(body.has_reference_instant(),
           body.has_axis_declination()) << body.name();
  CHECK_EQ(body.has_reference_instant(),
           body.has_reference_angle()) << body.name();
  CHECK_EQ(body.has_reference_instant(),
           body.has_angular_frequency()) << body.name();
  CHECK_EQ(body.has_j2(), body.has_reference_radius()) << body.name();
}

template<typename Frame>
not_null<std::unique_ptr<MassiveBody::Parameters>>
SolarSystem<Frame>::MakeMassiveBodyParameters(
    serialization::GravityModel::Body const& body) {
  return make_not_null_unique<MassiveBody::Parameters>(
      body.name(),
      ParseQuantity<GravitationalParameter>(body.gravitational_parameter()));
}

template<typename Frame>
not_null<std::unique_ptr<typename RotatingBody<Frame>::Parameters>>
SolarSystem<Frame>::MakeRotatingBodyParameters(
    serialization::GravityModel::Body const& body) {
  return make_not_null_unique<typename RotatingBody<Frame>::Parameters>(
      ParseQuantity<Length>(body.mean_radius()),
      ParseQuantity<Angle>(body.reference_angle()),
      JulianDate(body.reference_instant()),
      ParseQuantity<AngularFrequency>(body.angular_frequency()),
      ParseQuantity<Angle>(body.axis_right_ascension()),
      ParseQuantity<Angle>(body.axis_declination()));
}

template<typename Frame>
not_null<std::unique_ptr<typename OblateBody<Frame>::Parameters>>
SolarSystem<Frame>::MakeOblateBodyParameters(
    serialization::GravityModel::Body const& body) {
  return make_not_null_unique<typename OblateBody<Frame>::Parameters>(
      ParseQuantity<double>(body.j2()),
      ParseQuantity<Length>(body.reference_radius()));
}

template<typename Frame>
std::vector<not_null<std::unique_ptr<MassiveBody const>>>
SolarSystem<Frame>::MakeAllMassiveBodies() {
  std::vector<not_null<std::unique_ptr<MassiveBody const>>> bodies;
  for (auto const& pair : gravity_model_map_) {
    serialization::GravityModel::Body const* const body = pair.second;
    bodies.emplace_back(MakeMassiveBody(*body));
  }
  return bodies;
}

template<typename Frame>
std::vector<DegreesOfFreedom<Frame>>
SolarSystem<Frame>::MakeAllDegreesOfFreedom() {
  std::vector<DegreesOfFreedom<Frame>> degrees_of_freedom;
  for (auto const& pair : initial_state_map_) {
    serialization::InitialState::Body const* const body = pair.second;
    degrees_of_freedom.push_back(MakeDegreesOfFreedom(*body));
  }
  return degrees_of_freedom;
}

}  // namespace internal_solar_system
}  // namespace physics
}  // namespace principia
