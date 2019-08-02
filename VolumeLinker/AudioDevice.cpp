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

#include "AudioDevice.h"
#include "helpers.h"

AudioDevice::AudioDevice(
    size_t itemOffset,
    wil::com_ptr_nothrow<IMMDevice> pEndpoint)
{
    HRESULT hr;

    // Get the endpoint's ID string.
    wil::unique_cotaskmem_string pwszID;
    hr = pEndpoint->GetId(&pwszID);
    THROW_IF_COM_FAILED(hr, "Unable to retrieve audio endpoint ID.");

    // Open device property storage.
    wil::com_ptr_nothrow<IPropertyStore> pProps;
    hr = pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
    THROW_IF_COM_FAILED(hr, "Unable to open device property storage.");

    // Get the endpoint's friendy-name property.
    // NOTE: Multiple endpoints can have identical names (but IDs will always differ).
    wil::unique_prop_variant varName;
    hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
    THROW_IF_COM_FAILED(hr, "Unable to get name of audio endpoint.");

    m_iItemOffset = itemOffset;
    m_pEndpoint = std::move(pEndpoint);
    m_wsId = pwszID.get();
    m_wsName = varName.pwszVal;
}

AudioDevice::~AudioDevice()
{
}

wil::com_ptr_nothrow<IAudioEndpointVolume> AudioDevice::activateAudioEndpointVolume()
{
    HRESULT hr;

    // Create a COM object for the device, with the "endpoint volume" interface.
    wil::com_ptr_nothrow<IAudioEndpointVolume> pEndptVol;
    hr = m_pEndpoint->Activate(__uuidof(IAudioEndpointVolume),
        CLSCTX_ALL, NULL, (void**)& pEndptVol);
    THROW_IF_COM_FAILED(hr, "Unable to open device endpoint volume control.");

    return pEndptVol;
}

const size_t AudioDevice::getItemOffset()
{
    return m_iItemOffset;
}

const wstring& AudioDevice::getId()
{
    return m_wsId;
}

const LPWSTR AudioDevice::getIdMS()
{
    return (LPWSTR)m_wsId.c_str();
}

const wstring& AudioDevice::getName()
{
    return m_wsName;
}

const LPWSTR AudioDevice::getNameMS()
{
    return (LPWSTR)m_wsName.c_str();
}
