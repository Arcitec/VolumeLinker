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

// Sets the necessary values to target our application for Windows 7 SP1 and higher.
// NOTE: This file must be included BEFORE any Windows-related headers (to set their platform).
#include "targetver.h"

// Enables Common Controls 6, to activate modern Windows GUI visual styled controls.
// Technique from: https://docs.microsoft.com/en-us/windows/win32/controls/cookbook-overview
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Tell the linker to include additional, necessary libraries.
#pragma comment(lib, "Comctl32.lib")

// Exclude rarely-used stuff from Windows headers.
#define WIN32_LEAN_AND_MEAN

// Windows Headers.
#include <windows.h>

// C Runtime Headers.
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <string.h>
#include <locale.h> // Locale core.
#include <wchar.h> // Unicode locale settings.

// Standard C++ Library.
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <functional>
#include <string>

// More Windows Headers.
#include <commctrl.h> // Common Controls GUI code.
#include <windowsx.h> // Helpers for controls (such as GET_X_LPARAM/GET_Y_LPARAM).
#include <mmdeviceapi.h> // Interacting with audio devices.
#include <endpointvolume.h> // Interacting with volume mixers for audio devices.
#include <functiondiscoverykeys_devpkey.h> // Constants for "PKEY" device property storage.
#include <shellapi.h> // Necessary for NotifyIcon.

// Windows Implementation Library.
// NOTE: Must be included *after* all Windows API headers! See https://github.com/Microsoft/wil/wiki/RAII-resource-wrappers
#include <wil/resource.h> // https://github.com/Microsoft/wil/wiki/RAII-resource-wrappers
#include <wil/com.h> // https://github.com/microsoft/wil/wiki/WinRT-and-COM-wrappers

// Registry Wrapper.
#include <WinReg/WinReg.hpp>
