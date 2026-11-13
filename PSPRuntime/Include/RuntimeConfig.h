#pragma once

// ---------------------------------------------------------------------------
//  PSP Runtime - compile-time configuration knobs.
// ---------------------------------------------------------------------------

// Maximum number of objects that receive a per-frame GU draw call.
#define PSPRUNTIME_MAX_DRAW_OBJECTS 256

// Enables the on-screen console (SELECT + LTRIGGER).
#define PSPRUNTIME_ENABLE_CONSOLE 1

// Physics sub-stepping used by Physics::Step (fixed timestep accumulator).
#define PSPRUNTIME_PHYSICS_SUBSTEPS 2

// Runtime version string shown in the HUD.
#define PSPRUNTIME_VERSION "1.0.0"
