﻿
#pragma once

#include <functional>

#include "geometry/frame.hpp"
#include "geometry/named_quantities.hpp"
#include "ksp_plugin/manœuvre.hpp"
#include "physics/dynamic_frame.hpp"

namespace principia {
namespace ksp_plugin {
namespace internal_frames {

using geometry::Frame;
using physics::DynamicFrame;

// Thanks to KSP's madness, the reference frame of the celestial body orbited by
// the active vessel, occasionally rotating with its surface, occasionally
// nonrotating.
// The basis is that of Unity's "world space" (this is a left-handed basis).
// The origin is the ineffable origin of Unity's "world space".
using World = Frame<serialization::Frame::PluginTag,
                    serialization::Frame::WORLD, false>;

// Same as |World| but with the y and z axes switched through the looking-glass:
// it is a right-handed basis. "We're all mad here. I'm mad. You're mad."
using AliceWorld = Frame<serialization::Frame::PluginTag,
                         serialization::Frame::ALICE_WORLD, false>;

// The barycentric reference frame of the solar system.
using Barycentric = Frame<serialization::Frame::PluginTag,
                          serialization::Frame::BARYCENTRIC, true>;

// The axes are those of |Barycentre|.  The origin is the barycentre of the
// physics bubble, that is, the barycentre of the set of unpacked vessels.
// We identify the origin of |World| with it.
using Bubble = Frame<serialization::Frame::PluginTag,
                     serialization::Frame::BUBBLE, false>;

// The axes are those of |Barycentre|.  The origin is that of |World|.  This
// frame is used for degrees of freedom obtained after the physics simulation of
// the game has run, and before we perform our correction: the origin has no
// physical significance.
using ApparentBubble = Frame<serialization::Frame::PluginTag,
                             serialization::Frame::APPARENT_BUBBLE, false>;

// |Barycentric|, with its y and z axes swapped; the basis is left-handed.
using CelestialSphere = Frame<serialization::Frame::PluginTag,
                              serialization::Frame::CELESTIAL_SPHERE, true>;

// The surface frame of a celestial, with the x axis pointing to the origin of
// latitude and longitude, the y axis pointing to the pole with positive
// latitude, and the z axis oriented to form a left-handed basis.
using BodyWorld = Frame<serialization::Frame::PluginTag,
                        serialization::Frame::BODY_WORLD, false>;

// The frame used for the navball.  Its definition depends on the choice of a
// subclass of FrameField.  This frame is left-handed.
using Navball = Frame<serialization::Frame::PluginTag,
                      serialization::Frame::NAVBALL, false>;

// The frame used for trajectory plotting and manœuvre planning.  Its definition
// depends on the choice of a subclass of DynamicFrame.
using Navigation = Frame<serialization::Frame::PluginTag,
                         serialization::Frame::NAVIGATION, false>;

// A nonrotating referencence frame comoving with the sun with the same axes as
// |AliceWorld|. Since it is nonrotating (though not inertial), differences
// between velocities are consistent with those in an inertial reference frame.
// When |AliceWorld| rotates the axes are not fixed in the reference frame, so
// this (frame, basis) pair is inconsistent across instants. Operations should
// only be performed between simultaneous quantities, then converted to a
// consistent (frame, basis) pair before use.
using AliceSun = Frame<serialization::Frame::PluginTag,
                       serialization::Frame::ALICE_SUN, false>;

// Same as above, but with same axes as |World| instead of those of
// |AliceWorld|. The caveats are the same as for |AliceSun|.
using WorldSun = Frame<serialization::Frame::PluginTag,
                       serialization::Frame::WORLD_SUN, false>;

// Convenient instances of types from |physics| for the above frames.
using NavigationFrame = DynamicFrame<Barycentric, Navigation>;
using NavigationManœuvre = Manœuvre<Barycentric, Navigation>;

}  // namespace internal_frames

using internal_frames::AliceSun;
using internal_frames::AliceWorld;
using internal_frames::ApparentBubble;
using internal_frames::Barycentric;
using internal_frames::BodyWorld;
using internal_frames::Bubble;
using internal_frames::CelestialSphere;
using internal_frames::Navball;
using internal_frames::Navigation;
using internal_frames::NavigationFrame;
using internal_frames::NavigationManœuvre;
using internal_frames::World;
using internal_frames::WorldSun;

}  // namespace ksp_plugin
}  // namespace principia
