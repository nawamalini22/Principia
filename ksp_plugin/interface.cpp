﻿
#include "ksp_plugin/interface.hpp"

#include <cctype>
#include <cstring>
#include <iomanip>
#include <string>
#include <utility>
#include <vector>
#if OS_WIN
#define NOGDI
#include <windows.h>
#include <psapi.h>
#endif

#include "astronomy/epoch.hpp"
#include "base/array.hpp"
#include "base/hexadecimal.hpp"
#include "base/macros.hpp"
#include "base/not_null.hpp"
#include "base/optional_logging.hpp"
#include "base/pull_serializer.hpp"
#include "base/push_deserializer.hpp"
#include "base/version.generated.h"
#include "journal/method.hpp"
#include "journal/profiles.hpp"
#include "journal/recorder.hpp"
#include "ksp_plugin/part.hpp"
#include "physics/kepler_orbit.hpp"
#include "physics/solar_system.hpp"
#include "quantities/parser.hpp"
#include "serialization/astronomy.pb.h"
#include "serialization/ksp_plugin.pb.h"

namespace principia {
namespace interface {

using astronomy::J2000;
using base::Bytes;
using base::check_not_null;
using base::HexadecimalDecode;
using base::HexadecimalEncode;
using base::make_not_null_unique;
using base::PullSerializer;
using base::PushDeserializer;
using base::UniqueBytes;
using geometry::Displacement;
using geometry::RadiusLatitudeLongitude;
using geometry::Vector;
using geometry::Velocity;
using ksp_plugin::AliceSun;
using ksp_plugin::Barycentric;
using ksp_plugin::Part;
using ksp_plugin::PartId;
using ksp_plugin::World;
using physics::DegreesOfFreedom;
using physics::FrameField;
using physics::MassiveBody;
using physics::OblateBody;
using physics::RelativeDegreesOfFreedom;
using physics::RotatingBody;
using physics::SolarSystem;
using quantities::Acceleration;
using quantities::Force;
using quantities::ParseQuantity;
using quantities::Pow;
using quantities::Time;
using quantities::si::AstronomicalUnit;
using quantities::si::Day;
using quantities::si::Degree;
using quantities::si::Kilo;
using quantities::si::Metre;
using quantities::si::Newton;
using quantities::si::Radian;
using quantities::si::Second;
using quantities::si::Tonne;

namespace {

int const chunk_size = 64 << 10;
int const number_of_chunks = 8;

base::not_null<std::unique_ptr<RotatingBody<Barycentric>>> MakeRotatingBody(
    BodyParameters const& body_parameters) {
  // Logging operators would dereference a null C string.
  auto const make_optional_c_string = [](char const* const c_string) {
    return (c_string == nullptr) ? std::experimental::nullopt
                                 : std::experimental::make_optional(c_string);
  };
  LOG(INFO)
      << __FUNCTION__ << "\n"
      << NAMED(make_optional_c_string(body_parameters.gravitational_parameter))
      << "\n"
      << NAMED(body_parameters.reference_instant) << "\n"
      << NAMED(make_optional_c_string(body_parameters.mean_radius)) << "\n"
      << NAMED(make_optional_c_string(body_parameters.axis_right_ascension))
      << "\n"
      << NAMED(make_optional_c_string(body_parameters.axis_declination)) << "\n"
      << NAMED(make_optional_c_string(body_parameters.reference_angle)) << "\n"
      << NAMED(make_optional_c_string(body_parameters.angular_frequency))
      << "\n"
      << NAMED(make_optional_c_string(body_parameters.j2)) << "\n"
      << NAMED(make_optional_c_string(body_parameters.reference_radius));
  serialization::GravityModel::Body gravity_model;
  gravity_model.set_name(body_parameters.name);
  gravity_model.set_gravitational_parameter(
      body_parameters.gravitational_parameter);
  if (!std::isnan(body_parameters.reference_instant)) {
    gravity_model.set_reference_instant(body_parameters.reference_instant);
  }
  if (body_parameters.mean_radius != nullptr) {
    gravity_model.set_mean_radius(body_parameters.mean_radius);
  }
  if (body_parameters.axis_right_ascension != nullptr) {
    gravity_model.set_axis_right_ascension(
        body_parameters.axis_right_ascension);
  }
  if (body_parameters.axis_declination != nullptr) {
    gravity_model.set_axis_declination(body_parameters.axis_declination);
  }
  if (body_parameters.reference_angle != nullptr) {
    gravity_model.set_reference_angle(body_parameters.reference_angle);
  }
  if (body_parameters.angular_frequency!= nullptr) {
    gravity_model.set_angular_frequency(
        body_parameters.angular_frequency);
  }
  if (body_parameters.j2 != nullptr) {
    gravity_model.set_j2(body_parameters.j2);
  }
  if (body_parameters.reference_radius != nullptr) {
    gravity_model.set_reference_radius(body_parameters.reference_radius);
  }
  return SolarSystem<Barycentric>::MakeRotatingBody(gravity_model);
}

}  // namespace

// If |activate| is true and there is no active journal, create one and
// activate it.  If |activate| is false and there is an active journal,
// deactivate it.  Does nothing if there is already a journal in the desired
// state.  |verbose| causes methods to be output in the INFO log before being
// executed.
void principia__ActivateRecorder(bool const activate) {
  // NOTE: Do not journal!  You'd end up with half a message in the journal and
  // that would cause trouble.
  if (activate && !journal::Recorder::IsActivated()) {
    // Build a name somewhat similar to that of the log files.
    auto const now = std::chrono::system_clock::now();
    std::time_t const time = std::chrono::system_clock::to_time_t(now);
    std::tm* const localtime = std::localtime(&time);
    std::stringstream name;
    name << std::put_time(localtime, "JOURNAL.%Y%m%d-%H%M%S");
    journal::Recorder* const recorder =
        new journal::Recorder(std::experimental::filesystem::path("glog") /
                                  "Principia" / name.str());
    journal::Recorder::Activate(recorder);
  } else if (!activate && journal::Recorder::IsActivated()) {
    journal::Recorder::Deactivate();
  }
}

void principia__AdvanceTime(Plugin* const plugin,
                            double const t,
                            double const planetarium_rotation) {
  journal::Method<journal::AdvanceTime> m({plugin, t, planetarium_rotation});
  CHECK_NOTNULL(plugin);
  plugin->AdvanceTime(FromGameTime(*plugin, t), planetarium_rotation * Degree);
  return m.Return();
}

void principia__AdvanceParts(Plugin* const plugin, double const t) {
  journal::Method<journal::AdvanceParts> m({plugin, t});
  CHECK_NOTNULL(plugin);
  plugin->AdvanceParts(FromGameTime(*plugin, t));
  return m.Return();
}

// Calls |plugin->CelestialFromParent| with the arguments given.
// |plugin| must not be null.  No transfer of ownership.
QP principia__CelestialFromParent(Plugin const* const plugin,
                                  int const celestial_index) {
  journal::Method<journal::CelestialFromParent> m({plugin, celestial_index});
  CHECK_NOTNULL(plugin);
  return m.Return(ToQP(plugin->CelestialFromParent(celestial_index)));
}

double principia__CelestialInitialRotationInDegrees(Plugin const* const plugin,
                                                    int const celestial_index) {
  journal::Method<journal::CelestialInitialRotationInDegrees> m(
      {plugin, celestial_index});
  CHECK_NOTNULL(plugin);
  return m.Return(plugin->CelestialInitialRotation(celestial_index) / Degree);
}

WXYZ principia__CelestialRotation(Plugin const* const plugin, int const index) {
  journal::Method<journal::CelestialRotation> m({plugin, index});
  CHECK_NOTNULL(plugin);
  return m.Return(ToWXYZ(plugin->CelestialRotation(index).quaternion()));
}

double principia__CelestialRotationPeriod(
    Plugin const* const plugin,
    int const celestial_index) {
  journal::Method<journal::CelestialRotationPeriod> m(
      {plugin, celestial_index});
  CHECK_NOTNULL(plugin);
  return m.Return(plugin->CelestialRotationPeriod(celestial_index) / Second);
}

WXYZ principia__CelestialSphereRotation(Plugin const* const plugin) {
  journal::Method<journal::CelestialSphereRotation> m({plugin});
  CHECK_NOTNULL(plugin);
  return m.Return(ToWXYZ(plugin->CelestialSphereRotation().quaternion()));
}

QP principia__CelestialWorldDegreesOfFreedom(Plugin const* const plugin,
                                             int const index,
                                             PartId const part_at_origin) {
  journal::Method<journal::CelestialWorldDegreesOfFreedom> m(
      {plugin, index, part_at_origin});
  CHECK_NOTNULL(plugin);
  return m.Return(
      ToQP(plugin->CelestialWorldDegreesOfFreedom(index, part_at_origin)));
}

void principia__ClearTargetVessel(Plugin* const plugin) {
  journal::Method<journal::ClearTargetVessel> m({plugin});
  CHECK_NOTNULL(plugin);
  plugin->ClearTargetVessel();
  return m.Return();
}

double principia__CurrentTime(Plugin const* const plugin) {
  journal::Method<journal::CurrentTime> m({plugin});
  CHECK_NOTNULL(plugin);
  return m.Return(ToGameTime(*plugin, plugin->CurrentTime()));
}

// Deletes and nulls |*plugin|.
// |plugin| must not be null.  No transfer of ownership of |*plugin|, takes
// ownership of |**plugin|.
void principia__DeletePlugin(Plugin const** const plugin) {
  CHECK_NOTNULL(plugin);
  journal::Method<journal::DeletePlugin> m({plugin}, {plugin});
  LOG(INFO) << "Destroying Principia plugin";
  // We want to log before and after destroying the plugin since it is a pretty
  // significant event, so we take ownership inside a block.
  {
    TakeOwnership(plugin);
  }
  LOG(INFO) << "Plugin destroyed";
  return m.Return();
}

// Deletes and nulls |*native_string|.  |native_string| must not be null.  No
// transfer of ownership of |*native_string|, takes ownership of
// |**native_string|.
void principia__DeleteString(char const** const native_string) {
  journal::Method<journal::DeleteString> m({native_string}, {native_string});
  // This is a bit convoluted, but a |std::uint8_t const*| and a |char const*|
  // cannot be aliased.
  auto unsigned_string = reinterpret_cast<std::uint8_t const*>(*native_string);
  TakeOwnershipArray(&unsigned_string);
  *native_string = reinterpret_cast<char const*>(unsigned_string);
  return m.Return();
}

// The caller takes ownership of |**plugin| when it is not null.  No transfer of
// ownership of |*serialization| or |**deserializer|.  |*deserializer| and
// |*plugin| must be null on the first call and must be passed unchanged to the
// successive calls.  The caller must perform an extra call with
// |serialization_size| set to 0 to indicate the end of the input stream.  When
// this last call returns, |*plugin| is not null and may be used by the caller.
void principia__DeserializePlugin(char const* const serialization,
                                  int const serialization_size,
                                  PushDeserializer** const deserializer,
                                  Plugin const** const plugin) {
  journal::Method<journal::DeserializePlugin> m({serialization,
                                                 serialization_size,
                                                 deserializer,
                                                 plugin},
                                                {deserializer, plugin});
  CHECK_NOTNULL(serialization);
  CHECK_NOTNULL(deserializer);
  CHECK_NOTNULL(plugin);

  // Create and start a deserializer if the caller didn't provide one.
  if (*deserializer == nullptr) {
    LOG(INFO) << "Begin plugin deserialization";
    *deserializer = new PushDeserializer(chunk_size, number_of_chunks);
    auto message = make_not_null_unique<serialization::Plugin>();
    (*deserializer)->Start(
        std::move(message),
        [plugin](google::protobuf::Message const& message) {
          *plugin = Plugin::ReadFromMessage(
              static_cast<serialization::Plugin const&>(message)).release();
        });
  }

  // Decode the hexadecimal representation.
  std::uint8_t const* const hexadecimal =
      reinterpret_cast<std::uint8_t const*>(serialization);
  int const hexadecimal_size = serialization_size;
  int const byte_size = hexadecimal_size >> 1;
  // Ownership of the following pointer is transfered to the deserializer using
  // the callback to |Push|.
  std::uint8_t* bytes = new std::uint8_t[byte_size];
  HexadecimalDecode({hexadecimal, hexadecimal_size}, {bytes, byte_size});

  // Push the data, taking ownership of it.
  (*deserializer)->Push(Bytes(&bytes[0], byte_size),
                        [bytes]() { delete[] bytes; });

  // If the data was empty, delete the deserializer.  This ensures that
  // |*plugin| is filled.
  if (byte_size == 0) {
    LOG(INFO) << "End plugin deserialization";
    TakeOwnership(deserializer);
  }
  return m.Return();
}

// Calls |plugin->EndInitialization|.
// |plugin| must not be null.  No transfer of ownership.
void principia__EndInitialization(Plugin* const plugin) {
  journal::Method<journal::EndInitialization> m({plugin});
  CHECK_NOTNULL(plugin);
  plugin->EndInitialization();
  return m.Return();
}

void principia__ForgetAllHistoriesBefore(Plugin* const plugin,
                                         double const t) {
  journal::Method<journal::ForgetAllHistoriesBefore> m({plugin, t});
  CHECK_NOTNULL(plugin);
  plugin->ForgetAllHistoriesBefore(FromGameTime(*plugin, t));
  return m.Return();
}

void principia__FreeVesselsAndPartsAndCollectPileUps(Plugin* const plugin) {
  journal::Method<journal::FreeVesselsAndPartsAndCollectPileUps> m({plugin});
  CHECK_NOTNULL(plugin);
  plugin->FreeVesselsAndPartsAndCollectPileUps();
  return m.Return();
}

int principia__GetBufferDuration() {
  journal::Method<journal::GetBufferDuration> m;
  return m.Return(FLAGS_logbufsecs);
}

int principia__GetBufferedLogging() {
  journal::Method<journal::GetBufferedLogging> m;
  return m.Return(FLAGS_logbuflevel);
}

QP principia__GetPartActualDegreesOfFreedom(Plugin const* const plugin,
                                            PartId const part_id,
                                            PartId const part_at_origin) {
  journal::Method<journal::GetPartActualDegreesOfFreedom> m(
      {plugin, part_id, part_at_origin});
  CHECK_NOTNULL(plugin);
  return m.Return(
      ToQP(plugin->GetPartActualDegreesOfFreedom(part_id, part_at_origin)));
}

// Returns the frame last set by |plugin->SetPlottingFrame|.  No transfer of
// ownership.  The returned pointer is never null.
NavigationFrame const* principia__GetPlottingFrame(Plugin const* const plugin) {
  journal::Method<journal::GetPlottingFrame> m({plugin});
  return m.Return(CHECK_NOTNULL(plugin)->GetPlottingFrame());
}

int principia__GetStderrLogging() {
  journal::Method<journal::GetStderrLogging> m;
  return m.Return(FLAGS_stderrthreshold);
}

int principia__GetSuppressedLogging() {
  journal::Method<journal::GetSuppressedLogging> m;
  return m.Return(FLAGS_minloglevel);
}

int principia__GetVerboseLogging() {
  journal::Method<journal::GetVerboseLogging> m;
  return m.Return(FLAGS_v);
}

void principia__GetVersion(
    char const** const build_date,
    char const** const version) {
  journal::Method<journal::GetVersion> m({build_date, version});
  *CHECK_NOTNULL(build_date) = base::BuildDate;
  *CHECK_NOTNULL(version) = base::Version;
  return m.Return();
}

bool principia__HasEncounteredApocalypse(
    Plugin* const plugin,
    char const** const details) {
  journal::Method<journal::HasEncounteredApocalypse> m({plugin}, {details});
  // Ownership will be transfered to the marshmallow.
  std::string details_string;
  bool const has_encountered_apocalypse =
      CHECK_NOTNULL(plugin)->HasEncounteredApocalypse(&details_string);
  UniqueBytes allocated_details(details_string.size() + 1);
  std::memcpy(allocated_details.data.get(),
              details_string.data(),
              details_string.size() + 1);
  *CHECK_NOTNULL(details) =
      reinterpret_cast<char const*>(allocated_details.data.release());
  return m.Return(has_encountered_apocalypse);
}

bool principia__HasVessel(Plugin* const plugin,
                          char const* const vessel_guid) {
  journal::Method<journal::HasVessel> m({plugin,  vessel_guid});
  CHECK_NOTNULL(plugin);
  return m.Return(plugin->HasVessel(vessel_guid));
}

void principia__IncrementPartIntrinsicForce(Plugin* const plugin,
                                            PartId const part_id,
                                            XYZ const force_in_kilonewtons) {
  journal::Method<journal::IncrementPartIntrinsicForce> m(
      {plugin, part_id, force_in_kilonewtons});
  CHECK_NOTNULL(plugin)->IncrementPartIntrinsicForce(
      part_id,
      Vector<Force, World>(FromXYZ(force_in_kilonewtons) * Kilo(Newton)));
  return m.Return();
}

// Sets stderr to log INFO, and redirects stderr, which Unity does not log, to
// "<KSP directory>/stderr.log".  This provides an easily accessible file
// containing a sufficiently verbose log of the latest session, instead of
// requiring users to dig in the archive of all past logs at all severities.
// This archive is written to
// "<KSP directory>/glog/Principia/<SEVERITY>.<date>-<time>.<pid>",
// where date and time are in ISO 8601 basic format.
void principia__InitGoogleLogging() {
  if (google::IsGoogleLoggingInitialized()) {
    LOG(INFO) << "Google logging was already initialized, no action taken";
  } else {
#ifdef _MSC_VER
    FILE* file;
    freopen_s(&file, "stderr.log", "w", stderr);
#else
    std::freopen("stderr.log", "w", stderr);
#endif
    google::SetLogDestination(google::FATAL, "glog/Principia/FATAL.");
    google::SetLogDestination(google::ERROR, "glog/Principia/ERROR.");
    google::SetLogDestination(google::WARNING, "glog/Principia/WARNING.");
    google::SetLogDestination(google::INFO, "glog/Principia/INFO.");
    google::InitGoogleLogging("Principia");

    google::protobuf::SetLogHandler(
        [](google::protobuf::LogLevel const level,
           char const* const filename,
           int const line,
           std::string const& message) {
          LOG_AT_LEVEL(level) << "[" << filename << ":" << line << "] "
                              << message;
        });

    LOG(INFO) << "Initialized Google logging for Principia";
    LOG(INFO) << "Principia version " << principia::base::Version
              << " built on " << principia::base::BuildDate
              << " by " << principia::base::CompilerName
              << " version " << principia::base::CompilerVersion
              << " for " << principia::base::OperatingSystem
              << " " << principia::base::Architecture;
#if OS_WIN
  MODULEINFO module_info;
  memset(&module_info, 0, sizeof(module_info));
  CHECK(GetModuleInformation(GetCurrentProcess(),
                             GetModuleHandle(TEXT("principia")),
                             &module_info,
                             sizeof(module_info)));
  LOG(INFO) << "Base address is " << module_info.lpBaseOfDll;
#endif
  }
}

void principia__InsertCelestialAbsoluteCartesian(
    Plugin* const plugin,
    int const celestial_index,
    int const* const parent_index,
    BodyParameters const body_parameters,
    char const* const x,
    char const* const y,
    char const* const z,
    char const* const vx,
    char const* const vy,
    char const* const vz) {
  journal::Method<journal::InsertCelestialAbsoluteCartesian> m(
      {plugin,
       celestial_index,
       parent_index,
       body_parameters,
       x, y, z,
       vx, vy, vz});
  CHECK_NOTNULL(plugin);
  serialization::InitialState::Body initial_state;
  initial_state.set_x(x);
  initial_state.set_y(y);
  initial_state.set_z(z);
  initial_state.set_vx(vx);
  initial_state.set_vy(vy);
  initial_state.set_vz(vz);
  plugin->InsertCelestialAbsoluteCartesian(
      celestial_index,
      parent_index == nullptr ? std::experimental::nullopt
                              : std::experimental::make_optional(*parent_index),
      SolarSystem<Barycentric>::MakeDegreesOfFreedom(initial_state),
      MakeRotatingBody(body_parameters));
  return m.Return();
}

void principia__InsertCelestialJacobiKeplerian(
    Plugin* const plugin,
    int const celestial_index,
    int const* const parent_index,
    BodyParameters const body_parameters,
    KeplerianElements const* const keplerian_elements) {
  journal::Method<journal::InsertCelestialJacobiKeplerian> m(
      {plugin,
       celestial_index,
       parent_index,
       body_parameters,
       keplerian_elements});
  CHECK_NOTNULL(plugin);
  plugin->InsertCelestialJacobiKeplerian(
      celestial_index,
      parent_index == nullptr ? std::experimental::nullopt
                              : std::experimental::make_optional(*parent_index),
      keplerian_elements == nullptr
          ? std::experimental::nullopt
          : std::experimental::make_optional(
                FromKeplerianElements(*keplerian_elements)),
      MakeRotatingBody(body_parameters));
  return m.Return();
}

// Calls |plugin->InsertOrKeepVessel| with the arguments given.
// |plugin| must not be null.  No transfer of ownership.
void principia__InsertOrKeepVessel(Plugin* const plugin,
                                   char const* const vessel_guid,
                                   char const* const vessel_name,
                                   int const parent_index,
                                   bool const loaded,
                                   bool* inserted) {
  journal::Method<journal::InsertOrKeepVessel> m(
      {plugin, vessel_guid, vessel_name, parent_index, loaded}, {inserted});
  CHECK_NOTNULL(plugin);
  plugin->InsertOrKeepVessel(vessel_guid,
                             vessel_name,
                             parent_index,
                             loaded,
                             *inserted);
  return m.Return();
}

void principia__InsertOrKeepLoadedPart(
    Plugin* const plugin,
    PartId const part_id,
    char const* const name,
    double const mass_in_tonnes,
    char const* const vessel_guid,
    int const main_body_index,
    QP const main_body_world_degrees_of_freedom,
    QP const part_world_degrees_of_freedom) {
  journal::Method<journal::InsertOrKeepLoadedPart> m(
      {plugin,
       part_id,
       name,
       mass_in_tonnes,
       vessel_guid,
       main_body_index,
       main_body_world_degrees_of_freedom,
       part_world_degrees_of_freedom});
  CHECK_NOTNULL(plugin);
  plugin->InsertOrKeepLoadedPart(
      part_id,
      name,
      mass_in_tonnes * Tonne,
      vessel_guid,
      main_body_index,
      FromQP<DegreesOfFreedom<World>>(main_body_world_degrees_of_freedom),
      FromQP<DegreesOfFreedom<World>>(part_world_degrees_of_freedom));
  return m.Return();
}

// Calls |plugin->SetVesselStateOffset| with the arguments given.
// |plugin| must not be null.  No transfer of ownership.
void principia__InsertUnloadedPart(Plugin* const plugin,
                                   PartId const part_id,
                                   char const* const name,
                                   char const* const vessel_guid,
                                   QP const from_parent) {
  journal::Method<journal::InsertUnloadedPart> m(
      {plugin, part_id, name, vessel_guid, from_parent});
  CHECK_NOTNULL(plugin);
  plugin->InsertUnloadedPart(
      part_id,
      name,
      vessel_guid,
      FromQP<RelativeDegreesOfFreedom<AliceSun>>(from_parent));
  return m.Return();
}

bool principia__IsKspStockSystem(Plugin* const plugin) {
  journal::Method<journal::IsKspStockSystem> m({plugin});
  CHECK_NOTNULL(plugin);
  return m.Return(plugin->IsKspStockSystem());
}

// Exports |LOG(SEVERITY) << text| for fast logging from the C# adapter.
// This will always evaluate its argument even if the corresponding log severity
// is disabled, so it is less efficient than LOG(INFO).  It will not report the
// line and file of the caller.
void principia__LogError(char const* const text) {
  journal::Method<journal::LogError> m({text});
  LOG(ERROR) << text;
  return m.Return();
}

void principia__LogFatal(char const* const text) {
  journal::Method<journal::LogFatal> m({text});
  LOG(FATAL) << text;
  return m.Return();
}

void principia__LogInfo(char const* const text) {
  journal::Method<journal::LogInfo> m({text});
  LOG(INFO) << text;
  return m.Return();
}

void principia__LogWarning(char const* const text) {
  journal::Method<journal::LogWarning> m({text});
  LOG(WARNING) << text;
  return m.Return();
}

WXYZ principia__NavballOrientation(
    Plugin const* const plugin,
    XYZ const sun_world_position,
    XYZ const ship_world_position) {
  journal::Method<journal::NavballOrientation> m({plugin,
                                                  sun_world_position,
                                                  ship_world_position});
  CHECK_NOTNULL(plugin);
  auto const frame_field = plugin->NavballFrameField(
      FromXYZ<Position<World>>(sun_world_position));
  return m.Return(ToWXYZ(
      frame_field->FromThisFrame(
          FromXYZ<Position<World>>(ship_world_position)).quaternion()));
}

// Calls |plugin| to create a |NavigationFrame| using the given |parameters|.
NavigationFrame* principia__NewNavigationFrame(
    Plugin const* const plugin,
    NavigationFrameParameters const parameters) {
  journal::Method<journal::NewNavigationFrame> m({plugin, parameters});
  CHECK_NOTNULL(plugin);
  return m.Return(NewNavigationFrame(*plugin, parameters).release());
}

// Returns a pointer to a plugin constructed with the arguments given.
// The caller takes ownership of the result.
Plugin* principia__NewPlugin(char const* const game_epoch,
                             char const* const solar_system_epoch,
                             double const planetarium_rotation_in_degrees) {
  journal::Method<journal::NewPlugin> m({game_epoch,
                                         solar_system_epoch,
                                         planetarium_rotation_in_degrees});
  LOG(INFO) << "Constructing Principia plugin";
  not_null<std::unique_ptr<Plugin>> result = make_not_null_unique<Plugin>(
      J2000 + ParseQuantity<Time>(game_epoch),
      J2000 + ParseQuantity<Time>(solar_system_epoch),
      planetarium_rotation_in_degrees * Degree);
  LOG(INFO) << "Plugin constructed";
  return m.Return(result.release());
}

void principia__PrepareToReportCollisions(Plugin* const plugin) {
  journal::Method<journal::PrepareToReportCollisions> m({plugin});
  CHECK_NOTNULL(plugin)->PrepareToReportCollisions();
  return m.Return();
}

Iterator* principia__RenderedPrediction(Plugin* const plugin,
                                        char const* const vessel_guid,
                                        XYZ const sun_world_position) {
  journal::Method<journal::RenderedPrediction> m({plugin,
                                                  vessel_guid,
                                                  sun_world_position});
  CHECK_NOTNULL(plugin);
  auto const& prediction = plugin->GetVessel(vessel_guid)->prediction();
  auto rendered_trajectory = plugin->RenderBarycentricTrajectoryInWorld(
                                 prediction.Begin(),
                                 prediction.End(),
                                 FromXYZ<Position<World>>(sun_world_position));
  return m.Return(new TypedIterator<DiscreteTrajectory<World>>(
      std::move(rendered_trajectory),
      plugin));
}

void principia__RenderedPredictionApsides(Plugin const* const plugin,
                                          char const* const vessel_guid,
                                          int const celestial_index,
                                          XYZ const sun_world_position,
                                          Iterator** const apoapsides,
                                          Iterator** const periapsides) {
  journal::Method<journal::RenderedPredictionApsides> m(
      {plugin, vessel_guid, celestial_index, sun_world_position},
      {apoapsides, periapsides});
  CHECK_NOTNULL(plugin);
  auto const& prediction = plugin->GetVessel(vessel_guid)->prediction();
  std::unique_ptr<DiscreteTrajectory<World>> rendered_apoapsides;
  std::unique_ptr<DiscreteTrajectory<World>> rendered_periapsides;
  plugin->ComputeAndRenderApsides(celestial_index,
                                  prediction.Begin(),
                                  prediction.End(),
                                  FromXYZ<Position<World>>(sun_world_position),
                                  rendered_apoapsides,
                                  rendered_periapsides);
  *apoapsides = new TypedIterator<DiscreteTrajectory<World>>(
      check_not_null(std::move(rendered_apoapsides)),
      plugin);
  *periapsides = new TypedIterator<DiscreteTrajectory<World>>(
      check_not_null(std::move(rendered_periapsides)),
      plugin);
  return m.Return();
}

void principia__RenderedPredictionClosestApproaches(
    Plugin const* const plugin,
    char const* const vessel_guid,
    XYZ const sun_world_position,
    Iterator** const closest_approaches) {
  journal::Method<journal::RenderedPredictionClosestApproaches> m(
      {plugin, vessel_guid, sun_world_position},
      {closest_approaches});
  CHECK_NOTNULL(plugin);
  auto const& prediction = plugin->GetVessel(vessel_guid)->prediction();
  std::unique_ptr<DiscreteTrajectory<World>> rendered_closest_approaches;
  plugin->ComputeAndRenderClosestApproaches(
      prediction.Begin(),
      prediction.End(),
      FromXYZ<Position<World>>(sun_world_position),
      rendered_closest_approaches);
  *closest_approaches = new TypedIterator<DiscreteTrajectory<World>>(
      check_not_null(std::move(rendered_closest_approaches)),
      plugin);
  return m.Return();
}

void principia__RenderedPredictionNodes(Plugin const* const plugin,
                                        char const* const vessel_guid,
                                        XYZ const sun_world_position,
                                        Iterator** const ascending,
                                        Iterator** const descending) {
  journal::Method<journal::RenderedPredictionNodes> m(
      {plugin, vessel_guid, sun_world_position},
      {ascending, descending});
  CHECK_NOTNULL(plugin);
  auto const& prediction = plugin->GetVessel(vessel_guid)->prediction();
  std::unique_ptr<DiscreteTrajectory<World>> rendered_ascending;
  std::unique_ptr<DiscreteTrajectory<World>> rendered_descending;
  plugin->ComputeAndRenderNodes(prediction.Begin(),
                                prediction.End(),
                                FromXYZ<Position<World>>(sun_world_position),
                                rendered_ascending,
                                rendered_descending);
  *ascending = new TypedIterator<DiscreteTrajectory<World>>(
      check_not_null(std::move(rendered_ascending)),
      plugin);
  *descending = new TypedIterator<DiscreteTrajectory<World>>(
      check_not_null(std::move(rendered_descending)),
      plugin);
  return m.Return();
}

Iterator* principia__RenderedVesselTrajectory(Plugin const* const plugin,
                                              char const* const vessel_guid,
                                              XYZ const sun_world_position) {
  journal::Method<journal::RenderedVesselTrajectory> m({plugin,
                                                        vessel_guid,
                                                        sun_world_position});
  CHECK_NOTNULL(plugin);
  auto const& psychohistory = plugin->GetVessel(vessel_guid)->psychohistory();
  auto rendered_trajectory = plugin->RenderBarycentricTrajectoryInWorld(
                                 psychohistory.Begin(),
                                 psychohistory.End(),
                                 FromXYZ<Position<World>>(sun_world_position));
  return m.Return(new TypedIterator<DiscreteTrajectory<World>>(
      std::move(rendered_trajectory),
      plugin));
}

void principia__ReportCollision(Plugin const* const plugin,
                                PartId const part1_id,
                                PartId const part2_id) {
  journal::Method<journal::ReportCollision> m({plugin, part1_id, part2_id});
  CHECK_NOTNULL(plugin)->ReportCollision(part1_id, part2_id);
  return m.Return();
}

// Says hello, convenient for checking that calls to the DLL work.
char const* principia__SayHello() {
  journal::Method<journal::SayHello> m;
  return m.Return("Hello from native C++!");
}

// |plugin| must not be null.  The caller takes ownership of the result, except
// when it is null (at the end of the stream).  No transfer of ownership of
// |*plugin|.  |*serializer| must be null on the first call and must be passed
// unchanged to the successive calls; its ownership is not transferred.
char const* principia__SerializePlugin(Plugin const* const plugin,
                                       PullSerializer** const serializer) {
  journal::Method<journal::SerializePlugin> m({plugin, serializer},
                                              {serializer});
  CHECK_NOTNULL(plugin);
  CHECK_NOTNULL(serializer);

  // Create and start a serializer if the caller didn't provide one.
  if (*serializer == nullptr) {
    LOG(INFO) << "Begin plugin serialization";
    *serializer = new PullSerializer(chunk_size, number_of_chunks);
    auto message = make_not_null_unique<serialization::Plugin>();
    plugin->WriteToMessage(message.get());
    (*serializer)->Start(std::move(message));
  }

  // Pull a chunk.
  Bytes bytes;
  bytes = (*serializer)->Pull();

  // If this is the end of the serialization, delete the serializer and return a
  // nullptr.
  if (bytes.size == 0) {
    LOG(INFO) << "End plugin serialization";
    TakeOwnership(serializer);
    return m.Return(nullptr);
  }

  // Convert to hexadecimal and return to the client.
  std::int64_t const hexadecimal_size = (bytes.size << 1) + 1;
  UniqueBytes hexadecimal(hexadecimal_size);
  HexadecimalEncode(bytes, hexadecimal.get());
  hexadecimal.data.get()[hexadecimal_size - 1] = '\0';
  return m.Return(reinterpret_cast<char const*>(hexadecimal.data.release()));
}

// Sets the maximum number of seconds which logs may be buffered for.
void principia__SetBufferDuration(int const seconds) {
  journal::Method<journal::SetBufferDuration> m({seconds});
  FLAGS_logbufsecs = seconds;
  return m.Return();
}

// Log messages at a level |<= max_severity| are buffered.
// Log messages at a higher level are flushed immediately.
void principia__SetBufferedLogging(int const max_severity) {
  journal::Method<journal::SetBufferedLogging> m({max_severity});
  FLAGS_logbuflevel = max_severity;
  return m.Return();
}

void principia__SetMainBody(Plugin* const plugin, int const index) {
  journal::Method<journal::SetMainBody> m({plugin, index});
  CHECK_NOTNULL(plugin);
  plugin->SetMainBody(index);
  return m.Return();
}

void principia__SetPartApparentDegreesOfFreedom(Plugin* const plugin,
                                                PartId const part_id,
                                                QP const degrees_of_freedom) {
  journal::Method<journal::SetPartApparentDegreesOfFreedom> m(
      {plugin, part_id, degrees_of_freedom});
  CHECK_NOTNULL(plugin);
  plugin->SetPartApparentDegreesOfFreedom(
      part_id,
      FromQP<DegreesOfFreedom<World>>(degrees_of_freedom));
  return m.Return();
}

// |navigation_frame| must not be null.  No transfer of ownership of
// |*navigation_frame|, takes ownership of |**navigation_frame|, nulls
// |*navigation_frame|.
void principia__SetPlottingFrame(Plugin* const plugin,
                                 NavigationFrame** const navigation_frame) {
  journal::Method<journal::SetPlottingFrame> m({plugin, navigation_frame},
                                               {navigation_frame});
  CHECK_NOTNULL(plugin);
  plugin->SetPlottingFrame(TakeOwnership(navigation_frame));
  return m.Return();
}

void principia__SetPredictionLength(Plugin* const plugin,
                                    double const t) {
  journal::Method<journal::SetPredictionLength> m({plugin, t});
  CHECK_NOTNULL(plugin);
  plugin->SetPredictionLength(t * Second);
  return m.Return();
}

void principia__SetTargetVessel(Plugin* const plugin,
                                char const* const vessel_guid,
                                int const reference_body_index) {
  journal::Method<journal::SetTargetVessel> m(
      {plugin, vessel_guid, reference_body_index});
  CHECK_NOTNULL(plugin);
  plugin->SetTargetVessel(vessel_guid, reference_body_index);
  return m.Return();
}

// Make it so that all log messages of at least |min_severity| are logged to
// stderr (in addition to logging to the usual log file(s)).
void principia__SetStderrLogging(int const min_severity) {
  journal::Method<journal::SetStderrLogging> m({min_severity});
  // NOTE(egg): We could use |FLAGS_stderrthreshold| instead, the difference
  // seems to be a mutex.
  google::SetStderrLogging(min_severity);
  return m.Return();
}

// Log suppression level: messages logged at a lower level than this are
// suppressed.
void principia__SetSuppressedLogging(int const min_severity) {
  journal::Method<journal::SetSuppressedLogging> m({min_severity});
  FLAGS_minloglevel = min_severity;
  return m.Return();
}

// Show all VLOG(m) messages for |m <= level|.
void principia__SetVerboseLogging(int const level) {
  journal::Method<journal::SetVerboseLogging> m({level});
  FLAGS_v = level;
  return m.Return();
}

// Calls |plugin->UpdateCelestialHierarchy| with the arguments given.
// |plugin| must not be null.  No transfer of ownership.
void principia__UpdateCelestialHierarchy(Plugin const* const plugin,
                                         int const celestial_index,
                                         int const parent_index) {
  journal::Method<journal::UpdateCelestialHierarchy> m({plugin,
                                                        celestial_index,
                                                        parent_index});
  CHECK_NOTNULL(plugin);
  plugin->UpdateCelestialHierarchy(celestial_index, parent_index);
  return m.Return();
}

void principia__UpdatePrediction(Plugin const* const plugin,
                                 char const* const vessel_guid) {
  journal::Method<journal::UpdatePrediction> m({plugin, vessel_guid});
  CHECK_NOTNULL(plugin);
  plugin->UpdatePrediction(vessel_guid);
  return m.Return();
}

}  // namespace interface
}  // namespace principia
