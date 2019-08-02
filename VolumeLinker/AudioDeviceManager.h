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
#include "AudioDevice.h"
#include "AudioEndpointVolumeCallback.h"

using std::vector;

class AudioDeviceManager
{
private:
    int m_exitCode;
    GUID m_processGUID;
    HWND m_hDialog;
    ptrdiff_t m_iMuteCheckboxID;
    ptrdiff_t m_iVolumeSliderID;
    wil::com_ptr_nothrow<IMMDeviceEnumerator> m_pEnumerator;
    wil::com_ptr_nothrow<IMMDeviceCollection> m_pCollection;
    size_t m_collectionCount;
    std::vector<AudioDevice> m_audioDevices;
    bool m_bLinkActive;
    ptrdiff_t m_iMasterDeviceIdx;
    ptrdiff_t m_iSlaveDeviceIdx;
    wil::com_ptr_nothrow<IAudioEndpointVolume> m_pMasterEndptVol;
    wil::com_ptr_nothrow<IAudioEndpointVolume> m_pSlaveEndptVol;
    AudioEndpointVolumeCallback m_endpointVolumeCallback;

    void _updateDialog(float fMasterVolume, BOOL bMuted);
    bool _setSlaveVolume(float fVolume, BOOL bMuted) noexcept;
    void _onVolumeCallback(const PAUDIO_VOLUME_NOTIFICATION_DATA& pNotify);

public:
    AudioDeviceManager(GUID processGUID);
    ~AudioDeviceManager();
    int getExitCode() noexcept;
    const vector<AudioDevice>& getAudioDevices();
    const AudioDevice& getDevice(ptrdiff_t idx);
    bool isLinkActive() noexcept;
    void linkDevices(ptrdiff_t masterIdx, ptrdiff_t slaveIdx);
    void unlinkDevices() noexcept;
    ptrdiff_t getMasterDeviceIdx() noexcept;
    ptrdiff_t getSlaveDeviceIdx() noexcept;
    void setDialog(HWND hDlg, ptrdiff_t muteCheckbox, ptrdiff_t volumeSlider);
    bool setMasterVolume(float fVolume) noexcept;
    bool setMasterMute(BOOL bMuted) noexcept;
};
