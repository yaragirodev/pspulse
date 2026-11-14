#pragma once

// ---------------------------------------------------------------------------
//  PSPulse - shared build information
//  Used by both the desktop editor and the PSP runtime.
// ---------------------------------------------------------------------------

#define PSPULSE_NAME        "PSPulse"
#define PSPULSE_VERSION_MAJOR 1
#define PSPULSE_VERSION_MINOR 0
#define PSPULSE_VERSION_PATCH 0
#if defined(PSPULSE_BUILD_VERSION)
#define PSPULSE_VERSION_STR PSPULSE_BUILD_VERSION
#else
#define PSPULSE_VERSION_STR "1.0.0"
#endif

// Scene format identifier written into every .scene JSON file.
#define PSPULSE_SCENE_FORMAT "PSPulse.Scene"
#define PSPULSE_SCENE_VERSION 1
