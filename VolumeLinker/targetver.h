/*
 * This file is part of the Volume Linker project (https://github.com/VideoPlayerCode/VolumeLinker).
 * Copyright (C) 2019 VideoPlayerCode.
 *
 * Volume Linker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Volume Linker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Volume Linker.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#pragma once

// Build a backwards-compatible application so that it runs on Windows 7 SP1 or higher.
//
// NOTE: Win7 SP1 is the earliest possible version that is supported by the Win10 SDK.
// See https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk for OS list.
// And the reason why we don't use "an older SDK" to target even older operating systems is
// simply because only idiots run something older than Windows 7. ;-) Those older operating
// systems are no longer supported by Microsoft for a reason. They are old and full of exploits.
// And their older SDKs are full of unfixed bugs too, and would lower the quality of the Win7-10
// compatibility. So no thanks! If someone wants VolumeLinker, they need Win7+. ;-)
//
// NOTE: We've also set the "Project Properties -> C/C++ -> Code Generation -> Runtime Library"
// option for the RELEASE builds to "Multi-threaded (/MT)" instead of the default "Multi-
// threaded DLL (/MD)". This embeds the Visual C++ runtime into the executable so that people
// won't have to manually install the appropriate runtime (and not even KB2999226 either), which
// means that they CAN'T get the "This program can't start because MSVCP140.dll is missing" error.
// These settings together guarantee the most painless experience for older operating systems,
// and only grows the executable by about 100 KB, which is not much!
//
// NOTE: If we ever compile any library .DLL files in the same folder as the .EXE, they NEED to
// be built with the same static /MT runtime version, but even that's a bit risky because memory
// always has to be freed by the runtime that allocated it (since each statically compiled Visual
// C++ runtime would have its own individual heap), so in most cases, the ONLY working solution
// to be able to interact with custom .DLL files is to switch to /MD for everything.
//
// NOTE: "0x0601" is the raw value for "_WIN32_WINNT_WIN7". And yes we are supposed to manually
// copy the hex value rather than using the define (since it's defined in "SDKDDKVer.h", but
// we MUST set the new target platform value BEFORE we include that file).
#include <WinSDKVer.h>
#define _WIN32_WINNT 0x0601

// Including SDKDDKVer.h defines the available Windows platforms and feature levels.
//
// By default, it targets the highest available OS (such as Windows 10) if just included as-is.
// But here are Microsoft's instructions for targeting an older version (which we are doing):
//
// "If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h".
#include <SDKDDKVer.h>
