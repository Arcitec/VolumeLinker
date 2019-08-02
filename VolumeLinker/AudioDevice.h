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

#include "framework.h"

using std::wstring;

class AudioDevice
{
private:
    size_t m_iItemOffset;
    wil::com_ptr_nothrow<IMMDevice> m_pEndpoint;
    wstring m_wsId;
    wstring m_wsName;
public:
    AudioDevice(size_t itemOffset, wil::com_ptr_nothrow<IMMDevice> pEndpoint);
    ~AudioDevice();
    wil::com_ptr_nothrow<IAudioEndpointVolume> activateAudioEndpointVolume();
    const size_t getItemOffset();
    const wstring& getId();
    const LPWSTR getIdMS();
    const wstring& getName();
    const LPWSTR getNameMS();
};
