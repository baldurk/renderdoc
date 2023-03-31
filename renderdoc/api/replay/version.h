/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

///////////////////////////////////////////////////////////////////////////////////////
//
// Build configuration variables.
//
// When distributing RenderDoc builds please check these variables before building and
// set them appropriately. If you're building locally, the defaults are all fine.
//
// For windows builds, some of these variables are irrelevant and can be ignored.
//
// For non-windows builds, these variables have cmake settings that you can use to set
// them, and are designed to be set that way. See BUILD_VERSION_*.
//
// However if you want to you can locally modify this file (e.g. copy a file with some fixed values
// set, then just update the version numbers in the build process).
//

// To prevent a project rebuild cascading when the git commit changes, we declare a char array
// that's implemented in version.cpp, which is linked into the core module.
// It's 41 characters to allow 40 characters of commit hash plus trailing NULL.
// Then version.cpp is the only thing that needs to be rebuilt when the git commit changes
//
// Only available internally, external users should use RENDERDOC_GetCommitHash()
#if defined(RENDERDOC_EXPORTS)
extern "C" const char GitVersionHash[41];
#endif

// If this variable is set to 1, then this build is considered a stable version - based on a tagged
// version number upstream, possibly with some patches applied as necessary.
// Any other build whether it's including experimental local changes or just from the tip of the
// latest code at some other point should be considered unstable and leave this as 0.
#if !defined(RENDERDOC_STABLE_BUILD)
#define RENDERDOC_STABLE_BUILD 0
#endif

///////////////////////////////////////////////////////////////////////////////////////
//
// If you are distributing to the public, you should set values for these variables below.
//

// The friendly name of the distribution that packaged this build
#if !defined(DISTRIBUTION_NAME)
//#define DISTRIBUTION_NAME "DistributionName"
#endif

// An arbitrary distribution version string. If set, this should include the major and minor
// version numbers in it.
#if !defined(DISTRIBUTION_VERSION)
//#define DISTRIBUTION_VERSION "MAJ.MIN-foo.4b"
#endif

// Set to an URL or email of who produced this build and should be the first point of contact for
// any issues.
// If you're distributing builds for the public then do update this to point to your bugtracker or
// similar.
#if !defined(DISTRIBUTION_CONTACT)
//#define DISTRIBUTION_CONTACT "https://distribution.example/packages/renderdoc"
#endif

///////////////////////////////////////////////////////////////////////////////////////
//
// Internal or derived variables

// You should NOT enable this variable. This is used by upstream builds to determine whether
// this is an official build e.g. that should send crash reports.
#define RENDERDOC_OFFICIAL_BUILD 0

// The major and minor version that describe this build. These numbers are modified linearly
// upstream and should not be modified downstream. You can set DISTRIBUTION_VERSION to include any
// arbitrary release marker or package version you wish.
#define RENDERDOC_VERSION_MAJOR 1
#define RENDERDOC_VERSION_MINOR 27

#define RDOC_INTERNAL_VERSION_STRINGIZE2(a) #a
#define RDOC_INTERNAL_VERSION_STRINGIZE(a) RDOC_INTERNAL_VERSION_STRINGIZE2(a)

// string that's just "major.minor"
#define MAJOR_MINOR_VERSION_STRING                         \
  RDOC_INTERNAL_VERSION_STRINGIZE(RENDERDOC_VERSION_MAJOR) \
  "." RDOC_INTERNAL_VERSION_STRINGIZE(RENDERDOC_VERSION_MINOR)

// string that's the actual version number, either from the distribution or just vX.Y
#if defined(DISTRIBUTION_VERSION)
#define FULL_VERSION_STRING "v" DISTRIBUTION_VERSION
#else
#define FULL_VERSION_STRING "v" MAJOR_MINOR_VERSION_STRING
#endif
