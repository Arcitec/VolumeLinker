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

//-----------------------------------------------------------
// Client implementation of IAudioEndpointVolumeCallback
// interface. When a method in the IAudioEndpointVolume
// interface changes the volume level or muting state of the
// endpoint device, the change initiates a call to the
// client's IAudioEndpointVolumeCallback::OnNotify method.
//-----------------------------------------------------------
class AudioEndpointVolumeCallback : public IAudioEndpointVolumeCallback
{
    LONG m_references; // Reference counter.
    std::function<void(const PAUDIO_VOLUME_NOTIFICATION_DATA&)> m_externalCallback;

public:
    AudioEndpointVolumeCallback() :
        m_references(1) // Set counter to 1 reference when constructed.
    {
    }

    ~AudioEndpointVolumeCallback()
    {
    }

    // IUnknown methods -- AddRef, Release, and QueryInterface

    ULONG STDMETHODCALLTYPE AddRef() noexcept
    {
        return InterlockedIncrement(&m_references);
    }

    ULONG STDMETHODCALLTYPE Release() noexcept
    {
        ULONG const refCount = InterlockedDecrement(&m_references);
        if (0 == refCount) {
            delete this;
        }
        return refCount;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(
        REFIID riid,
        VOID** ppvInterface) noexcept
    {
        if (IID_IUnknown == riid) {
            AddRef();
            *ppvInterface = static_cast<IUnknown*>(this);
        }
        else if (__uuidof(IAudioEndpointVolumeCallback) == riid) {
            AddRef();
            *ppvInterface = static_cast<IAudioEndpointVolumeCallback*>(this);
        }
        else {
            *ppvInterface = NULL;
            return E_NOINTERFACE;
        }
        return S_OK;
    }

    // Callback method for endpoint-volume-change notifications.

    HRESULT STDMETHODCALLTYPE OnNotify(
        PAUDIO_VOLUME_NOTIFICATION_DATA pNotify)
    {
        if (pNotify == NULL) {
            return E_INVALIDARG;
        }

#ifdef _DEBUG
        OutputDebugStringA(std::string("Callback:" + std::to_string(pNotify->fMasterVolume) + " " + (pNotify->bMuted ? "M" : "_") + "\n").c_str());
#endif

        // Execute any registered callback, but BLOCK any exceptions it throws.
        try {
            if (m_externalCallback) {
                m_externalCallback(pNotify); // Throws if invalid (or throwing) callback.
            }
        }
        catch (...) {}

        return S_OK;
    }

    // Registering or unregistering external callback.

    void registerCallback(
        std::function<void(const PAUDIO_VOLUME_NOTIFICATION_DATA&)> callback)
    {
        m_externalCallback = callback;
    }

    void unregisterCallback()
    {
        m_externalCallback = nullptr;
    }
};
