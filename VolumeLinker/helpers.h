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

#define THROW_IF_COM_FAILED(hr, msg)  \
              if (FAILED(hr))  \
                { throw std::runtime_error(msg); }

// Maximum volume level on trackbar.
#define MAX_VOL 100

// Debugging helpers.
#define OUTPUT_DEBUG_VALUE_TOSTRING(any) \
            OutputDebugStringA((std::to_string(any) + "\r\n").c_str());
#define OUTPUT_DEBUG_MSGBOXA(txt) \
            MessageBoxA(NULL, txt, "Message", MB_OK);
#define OUTPUT_DEBUG_MSGBOXW(txt) \
            MessageBoxW(NULL, txt, L"Message", MB_OK);
