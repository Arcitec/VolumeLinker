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

#include "AudioDeviceManager.h"
#include "helpers.h"

using std::move;

AudioDeviceManager::AudioDeviceManager(
    GUID processGUID)
{
    HRESULT hr;

    // The exit code from the class will always be zero (no error) unless the callback failed.
    m_exitCode = 0;

    // Save the GUID that we will identify our COM calls with.
    if (processGUID == GUID_NULL) {
        throw std::runtime_error("Invalid process GUID given to AudioDeviceManager.");
    }
    m_processGUID = processGUID;

    // At the moment we don't have any dialog handle that we're attached to.
    m_hDialog = NULL;
    m_iMuteCheckboxID = 0;
    m_iVolumeSliderID = 0;

    // Get enumerator for audio endpoint devices.
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
        NULL, CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator),
        (void**)& m_pEnumerator);
    THROW_IF_COM_FAILED(hr, "Unable to create audio device enumerator.");

    // Get all audio-rendering devices (except ones that are disabled/not present).
    hr = m_pEnumerator->EnumAudioEndpoints(
        eRender, DEVICE_STATE_ACTIVE | DEVICE_STATE_UNPLUGGED, &m_pCollection);
    THROW_IF_COM_FAILED(hr, "Unable to enumerate audio devices.");

    // Count the discovered devices.
    UINT count;
    hr = m_pCollection->GetCount(&count);
    THROW_IF_COM_FAILED(hr, "Unable to count audio devices in collection.");
    m_collectionCount = static_cast<size_t>(count);
    if (m_collectionCount == 0) {
        throw std::runtime_error("No audio devices found.");
    }

    for (size_t i = 0; i < m_collectionCount; ++i) {
        // Get pointer to endpoint number i.
        wil::com_ptr_nothrow<IMMDevice> pEndpoint;
        hr = m_pCollection->Item(static_cast<UINT>(i), &pEndpoint);
        THROW_IF_COM_FAILED(hr, "Unable to retrieve an audio endpoint.");

        // Save device to vector.
        AudioDevice device(i, move(pEndpoint));
        m_audioDevices.push_back(move(device));
    }

    // Sort the devices by name in ascending order (case SENSITIVE).
    std::sort(m_audioDevices.begin(), m_audioDevices.end(), [](AudioDevice& a, AudioDevice& b) -> bool
        {
            // NOTE: This requires that "_wsetlocale" has been executed to set the program's
            // locale, otherwise it uses the "C" (ANSI) basic locale by default and can't
            // achieve a case-insensitive comparison of international letters.
            return _wcsicmp(a.getName().c_str(), b.getName().c_str()) < 0; // Case Insensitive (Microsoft implementation).
        });

    // Validate collection sizes.
    if (m_collectionCount != m_audioDevices.size()) {
        throw std::runtime_error("Unable to retrieve all audio device information.");
    }

    // There is no link at the beginning.
    m_bLinkActive = false;
    m_iMasterDeviceIdx = -1;
    m_iSlaveDeviceIdx = -1;
    m_pMasterEndptVol = nullptr;
    m_pSlaveEndptVol = nullptr;

    // Register a callback lambda which calls our class instance's volume callback.
    m_endpointVolumeCallback.registerCallback(
        [this](const PAUDIO_VOLUME_NOTIFICATION_DATA& pNotify) -> void {
            this->_onVolumeCallback(pNotify);
        });
}

AudioDeviceManager::~AudioDeviceManager()
{
    // Ensure that any callback-link between devices is unloaded first, before regular destruction.
    this->unlinkDevices();

    // Remove ourselves as callback from the inner AudioEndpointVolumeCallback class.
    // NOTE: Probably completely pointless since that object and its reference to us
    // would get destroyed in reverse creation-order and properly release anyway.
    m_endpointVolumeCallback.unregisterCallback();
}

int AudioDeviceManager::getExitCode() noexcept
{
    return m_exitCode;
}

const vector<AudioDevice>& AudioDeviceManager::getAudioDevices()
{
    return m_audioDevices;
}

const AudioDevice& AudioDeviceManager::getDevice(
    ptrdiff_t idx)
{
    if (idx < 0) {
        throw std::runtime_error("Negative device number requested.");
    }

    try {
        return m_audioDevices.at(static_cast<size_t>(idx));
    }
    catch (const std::out_of_range&) {
        throw std::runtime_error("Invalid device number requested.");
    }
}

bool AudioDeviceManager::isLinkActive() noexcept
{
    return m_bLinkActive;
}

void AudioDeviceManager::linkDevices(
    ptrdiff_t masterIdx,
    ptrdiff_t slaveIdx)
{
    HRESULT hr;

    // Ensure that any existing link is broken first.
    this->unlinkDevices();

    // Don't allow circular links between the same device.
    if (masterIdx == slaveIdx) {
        throw std::runtime_error("Cannot link device to itself.");
    }

    // Retrieve the devices (throws if the indices are invalid, negative, etc).
    AudioDevice masterDevice = this->getDevice(masterIdx);
    AudioDevice slaveDevice = this->getDevice(slaveIdx);

    // Connect to the "endpoint volume control" interfaces for both devices.
    m_pMasterEndptVol = masterDevice.activateAudioEndpointVolume();
    m_pSlaveEndptVol = slaveDevice.activateAudioEndpointVolume();

    // Register our callback to get volume/mute change notifications for the master device.
    hr = m_pMasterEndptVol->RegisterControlChangeNotify(
        (IAudioEndpointVolumeCallback*)& m_endpointVolumeCallback);
    THROW_IF_COM_FAILED(hr, "Unable to register master audio endpoint volume callback.");

    // Signal the fact that the link is now active (since the callback registration succeeded).
    m_bLinkActive = true;
    m_iMasterDeviceIdx = masterIdx;
    m_iSlaveDeviceIdx = slaveIdx;

    // Get the master device's current volume and mute-state.
    BOOL bMuted;
    float fMasterVolume;
    hr = m_pMasterEndptVol->GetMute(&bMuted);
    if (FAILED(hr)) {
        this->unlinkDevices();
        throw std::runtime_error("Failed to retrieve master device's volume state. Link could not be established.");
    }
    hr = m_pMasterEndptVol->GetMasterVolumeLevelScalar(&fMasterVolume);
    if (FAILED(hr)) {
        this->unlinkDevices();
        throw std::runtime_error("Failed to retrieve master device's mute state. Link could not be established.");
    }

    // Apply the same volume to the slave-device immediately.
    bool success = this->_setSlaveVolume(fMasterVolume, bMuted);
    if (!success) {
        this->unlinkDevices();
        throw std::runtime_error("Failed to sync master volume to slave device. Link could not be established.");
    }

    // Lastly, update the GUI immediately to display the master device's volume/mute state.
    this->_updateDialog(fMasterVolume, bMuted);
}

void AudioDeviceManager::unlinkDevices() noexcept
{
    // Unregister the master device's callback.
    if (m_bLinkActive && m_pMasterEndptVol) {
        // NOTE: Ignoring HRESULT since the only possible error is that the pointer is NULL.
        m_pMasterEndptVol->UnregisterControlChangeNotify(
            (IAudioEndpointVolumeCallback*)& m_endpointVolumeCallback);
    }

    // Clear all device pointers (releases the old COM resources).
    m_bLinkActive = false;
    m_iMasterDeviceIdx = -1;
    m_iSlaveDeviceIdx = -1;
    m_pMasterEndptVol = nullptr;
    m_pSlaveEndptVol = nullptr;
}

ptrdiff_t AudioDeviceManager::getMasterDeviceIdx() noexcept
{
    return m_iMasterDeviceIdx;
}

ptrdiff_t AudioDeviceManager::getSlaveDeviceIdx() noexcept
{
    return m_iSlaveDeviceIdx;
}

void AudioDeviceManager::setDialog(
    HWND hDlg,
    ptrdiff_t muteCheckboxID,
    ptrdiff_t volumeSliderID)
{
    m_hDialog = hDlg;
    m_iMuteCheckboxID = muteCheckboxID;
    m_iVolumeSliderID = volumeSliderID;
}

void AudioDeviceManager::_updateDialog(
    float fMasterVolume,
    BOOL bMuted)
{
    if (m_hDialog == NULL) {
        return;
    }

    PostMessage(GetDlgItem(m_hDialog, static_cast<int>(m_iMuteCheckboxID)),
        BM_SETCHECK, (bMuted) ? BST_CHECKED : BST_UNCHECKED, 0);

    // Calculate slider position (while rounding halves up).
    ptrdiff_t sliderVolume = static_cast<ptrdiff_t>((static_cast<double>(MAX_VOL) * fMasterVolume) + 0.5);
    if (sliderVolume < 0) { sliderVolume = 0; }
    else if (sliderVolume > MAX_VOL) { sliderVolume = MAX_VOL; }

    PostMessage(GetDlgItem(m_hDialog, static_cast<int>(m_iVolumeSliderID)),
        TBM_SETPOS, TRUE, static_cast<LPARAM>(static_cast<UINT32>(sliderVolume)));
}

bool AudioDeviceManager::setMasterVolume(
    float fVolume) noexcept
{
    // If we don't have any device, automatically return true.
    if (!m_pMasterEndptVol) {
        return true;
    }

    // Attempt to set the volume, and only return true if successful.
    // NOTE: Will also propagate to slave device, via the master device's callback.
    HRESULT hr;
    hr = m_pMasterEndptVol->SetMasterVolumeLevelScalar(fVolume, &m_processGUID);
    if (FAILED(hr)) { return false; }

    return true;
}

bool AudioDeviceManager::setMasterMute(
    BOOL bMuted) noexcept
{
    // If we don't have any device, automatically return true.
    if (!m_pMasterEndptVol) {
        return true;
    }

    // Attempt to set the mute-state, and only return true if successful.
    // NOTE: Will also propagate to slave device, via the master device's callback.
    HRESULT hr;
    hr = m_pMasterEndptVol->SetMute(bMuted, &m_processGUID);
    if (FAILED(hr)) { return false; }

    return true;
}

bool AudioDeviceManager::_setSlaveVolume(
    float fVolume,
    BOOL bMuted) noexcept
{
    // If we don't have any device, automatically return true.
    if (!m_pSlaveEndptVol) {
        return true;
    }

#ifdef _DEBUG
    OutputDebugStringA(std::string("SetSlave:" + std::to_string(fVolume) + " " + (bMuted ? "M" : "_") + "\n").c_str());
#endif

    // Attempt to set the volume and mute-state, and only return true if both succeeded.
    HRESULT hr;
    hr = m_pSlaveEndptVol->SetMasterVolumeLevelScalar(fVolume, &m_processGUID);
    if (FAILED(hr)) { return false; }
    hr = m_pSlaveEndptVol->SetMute(bMuted, &m_processGUID);
    if (FAILED(hr)) { return false; }

    return true;
}

void AudioDeviceManager::_onVolumeCallback(
    const PAUDIO_VOLUME_NOTIFICATION_DATA& pNotify)
{
    // Do nothing if the callback was somehow triggered while we don't have any linked master/slave devices.
    if (!m_bLinkActive || !m_pMasterEndptVol || !m_pSlaveEndptVol) {
        return;
    }

    // Update dialog if the volume event wasn't sent by our own program...
    if (pNotify->guidEventContext != m_processGUID) {
        this->_updateDialog(pNotify->fMasterVolume, pNotify->bMuted);
    }

    // Sync the volume to the slave device (regardless of who changed the master device's volume)...
    bool success = this->_setSlaveVolume(pNotify->fMasterVolume, pNotify->bMuted);
    if (!success && m_hDialog != NULL) {
        // Quit the whole program (close the main dialog) if slave volume failed.
        // NOTE: Posting a WM_CLOSE is the correct way to "close a window". However, it doesn't support
        // signaling that there was an error. So we'll transmit that by setting our class "exit code" value.
        m_exitCode = 1;
        MessageBoxW(m_hDialog, L"Failed to sync master volume to slave device in callback. The program will now exit.",
            L"Fatal Error", MB_OK);
        PostMessage(m_hDialog, WM_CLOSE, 0, 0);
        return;
    }
}
