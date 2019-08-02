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

#include "framework.h"
#include "resource.h"
#include "helpers.h"
#include "AudioDeviceManager.h"

using std::vector;
using std::wstring;

// Startup Option: Start the application minimized (hidden window).
static bool g_optStartMinimized = false;

// Startup Option: Always attempt to link devices (if both master and slave are selected)
// at startup, even if they were unlinked while the program was last closed. This attempt
// is done silently (no error popup boxes).
// NOTE: This option does not activate "g_saveChanges", so if the last manually set state
// was "unlinked", the registry state will still say "unlinked" until the user manually
// toggles the master/slave/link-settings. This protects against inadvertent overwriting.
static bool g_optForceLink = false;

// Program instance handle.
static HINSTANCE g_hInstance = NULL;

// Dialog handle from dialog box procedure.
static HWND g_hDlg = NULL;

// Whether we have a notification area icon.
static bool g_hasNotifyIcon = false;

// Data for the notificiation area icon.
static NOTIFYICONDATA g_notifyIconData = {};

// Context-menu for notification area icon.
static HMENU g_hTrayMenu = NULL;

// Application and tray icons.
static HICON g_iconLargeMain = NULL; // Large main icon.
static HICON g_iconSmallMain = NULL; // Small main icon.
static HICON g_iconLargeDisabled = NULL; // Large disabled icon.
static HICON g_iconSmallDisabled = NULL; // Small disabled icon.

// Audio device manager for the whole program.
static std::unique_ptr<AudioDeviceManager> g_deviceManager = nullptr;

// Determines whether the user has MANUALLY changed any settings (which then needs saving).
static bool g_saveChanges = false;

// Access rights for registry access (telling the 32-bit version to use 64-bit keys if opened on Win64).
static const REGSAM g_regDesiredAccess = KEY_READ | KEY_WRITE | KEY_WOW64_64KEY;

// Registry key and value names.
static const auto g_regHKey = HKEY_CURRENT_USER;
static const auto g_regSoftwareKey = wstring(L"SOFTWARE\\VolumeLinker");
static const auto g_regMasterDevice = wstring(L"MasterDevice");
static const auto g_regSlaveDevice = wstring(L"SlaveDevice");
static const auto g_regLinkActive = wstring(L"LinkActive");

#define APP_WM_ICONNOTIFY (WM_APP + 1)
#define APP_WM_BRINGTOFRONT (WM_APP + 6400)
#define APP_WINDOW_TITLE_32 L"Volume Linker (32-bit)"
#define APP_WINDOW_TITLE_64 L"Volume Linker (64-bit)"

// Forward declarations of other functions.
void ExitCleanup() noexcept;
void Dlg_ShowAndForeground() noexcept;
ptrdiff_t Dlg_GetDropdownSelection(int dlgItem) noexcept;
void Dlg_ShowLinkState() noexcept;
void Dlg_LinkDevices(bool showErrors);
void Dlg_UnlinkDevices() noexcept;
bool Dlg_SaveSettings() noexcept;
BOOL CALLBACK AboutDlgProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK QuitDlgProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    // Refuse to open multiple instances of the program.
    // NOTE: The mutex is reference-counted by the OS and perfectly auto-released AFTER our
    // process is terminated. So we will NOT manually ReleaseMutex/CloseHandle.
    // NOTE: A "Global" mutex means that all users can see it (ie. with "fast user switching").
    // NOTE: This "only allow one process on machine" locking is extremely important, to avoid
    // multiple "volume link" callbacks all doing the same (or opposite) things and conflicting.
    auto singleInstanceMutex = CreateMutex(NULL, TRUE, L"Global\\VideoPlayerCode.VolumeLinker");
    if (singleInstanceMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        // Attempt to find the other instance's window (by its title).
        // NOTE: This can have false positives if some other window uses the exact same title.
        // NOTE: On Windows 10+ with "virtual desktop", this also finds the app on other desktops.
        // NOTE: If the application is running on another user's account during "fast user switching",
        // this will NOT find THEIR window. (Verified on Win7 SP1, Win8.1 and Win10.)
        HWND existingWindow = FindWindow(NULL, APP_WINDOW_TITLE_64);
        if (!existingWindow) {
            existingWindow = FindWindow(NULL, APP_WINDOW_TITLE_32);
        }
        if (existingWindow) {
            // We've found what we assume is our other instance's window, so we'll post (async)
            // a custom message to make that window activate itself. We use a very conservative,
            // custom WM_APP-based message that is unlikely to ever trigger unexpected behavior
            // if we accidentally send this to the wrong window. The WM_APP messages are within
            // a non-OS range by themselves, and our special one is deep within the WM_APP range.
            PostMessage(existingWindow, APP_WM_BRINGTOFRONT, 0, 0);
        }
        else {
            // The mutex is taken but we can't find the window. Most likely running as another user,
            // with "fast user switching". Warn user that their computer can only run ONE instance.
            MessageBoxW(NULL, L"You can only have one active instance of Volume Linker per computer.\r\nPerhaps it's still running on another user's account?", L"Fatal Error", MB_OK);
        }
        return 0;
    }

    // Exit code.
    int exitCode = 0;

    // Set the locale for string handling/comparison/sorting purposes (mainly for _wcsicmp).
    // NOTE: We'll automatically use the system's locale, by passing in an empty value.
    _wsetlocale(LC_ALL, L"");

    // Read command line parameters.
    for (int i = 1; i < __argc; ++i) { // Skips "0" (the executable path).
        if (_wcsicmp(__wargv[i], L"/m") == 0 ||
            _wcsicmp(__wargv[i], L"/minimized") == 0 ||
            _wcsicmp(__wargv[i], L"/minimize") == 0)
        {
            g_optStartMinimized = true;
        }
        else if (_wcsicmp(__wargv[i], L"/l") == 0 ||
            _wcsicmp(__wargv[i], L"/link") == 0)
        {
            g_optForceLink = true;
        }
    }

    // Save program instance handle to global variable.
    g_hInstance = hInstance;

    // Open COM connection for current thread, in single-threaded mode.
    HRESULT hr;
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    // Guarantees that "CoUninitialize" is called when the program ends.
    // NOTE: It's okay that this is called even if CoInitializeEx failed above.
    wil::unique_couninitialize_call cleanup;

    try {
        // Verify that the COM connection was opened.
        THROW_IF_COM_FAILED(hr, "Unable to initialize COM connection.");

        // Create a random COM event-context GUID to identify our application process.
        GUID processGUID = GUID_NULL;
        hr = CoCreateGuid(&processGUID);
        THROW_IF_COM_FAILED(hr, "Unable to create COM process GUID.");

        // Connect to the audio COM server and retrieve list of devices.
        // NOTE: Saves a global pointer to the class for use by our dialog.
        g_deviceManager = std::make_unique<AudioDeviceManager>(processGUID);

        // Initialize and register Windows GUI control classes (Common Controls 6+).
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_WIN95_CLASSES; // All standard classes.
        if (!InitCommonControlsEx(&icex)) {
            throw std::runtime_error("Unable to initialize common controls library.");
        }

        // Load all application icons.
        // NOTE: Automatically loads (with scaling if necessary) appropriate icon sizes based on screen DPI.
        LoadIconMetric(hInstance, MAKEINTRESOURCE(IDI_MAINICON), LIM_LARGE, &g_iconLargeMain);
        LoadIconMetric(hInstance, MAKEINTRESOURCE(IDI_MAINICON), LIM_SMALL, &g_iconSmallMain);
        LoadIconMetric(hInstance, MAKEINTRESOURCE(IDI_DISABLEDICON), LIM_LARGE, &g_iconLargeDisabled);
        LoadIconMetric(hInstance, MAKEINTRESOURCE(IDI_DISABLEDICON), LIM_SMALL, &g_iconSmallDisabled);

        // Create our main program dialog.
        // NOTE: Unlike the modal DialogBox(), which contains an internal message loop and
        // doesn't return until the dialog is closed, the CreateDialog() technique actually
        // returns immediately and the messages are processed via the program's main message
        // loop instead.
        // NOTE: Before the function returns, it executes the callback with WM_INITDIALOG,
        // and provides the exact dialog HWND handle to that function, and our code in that
        // event then writes the handle to g_hDlg. However, we'll assign it here too (yes,
        // the exact same value), because it allows us to use inspect the return value to
        // detect when the dialog failed to create (in which case WM_INITDIALOG wouldn't run).
        // NOTE: Because our dialog lacks the "WS_VISIBLE" style, it won't be auto-shown
        // by the CreateDialog function. That allows us to manually control initial visibility!
        // More details:
        // https://docs.microsoft.com/en-us/windows/win32/dlgbox/using-dialog-boxes#creating-a-modeless-dialog-box
        // https://docs.microsoft.com/en-us/windows/win32/winmsg/using-messages-and-message-queues
        g_hDlg = CreateDialog(hInstance, MAKEINTRESOURCE(DLG_VOLUMELINKER), NULL, (DLGPROC)MainDlgProc);
        if (g_hDlg == NULL) {
            throw std::runtime_error("Unable to load application interface.");
        }

        // Next, it's time to show the window, EXCEPT if the user has requested "start minimized".
        if (!g_optStartMinimized) {
            ShowWindow(g_hDlg, SW_SHOW);
        }

        // We must now implement a standard Windows message pump loop...
        // NOTE: All callbacks run within this main GUI thread, which means that
        // the callbacks can use COM resources owned by the main thread (this one).
        // NOTE: If anything within the callback throws and isn't caught in there,
        // it will bubble up to us and be handled by our exception-catcher below,
        // and the program will then exit gracefully... However, CERTAIN MESSAGES
        // do not allow the stack to unwind and will NOT bubble up to us! So all
        // error handling should optimally be done inside the message handler!
        // NOTE: The GetMessage() loops until it sees WM_QUIT (by PostQuitMessage()).

        MSG msg;
        BOOL bRet;

        while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
        {
            if (bRet == -1)
            {
                // A code of "-1" means that there's a serious error which prevented
                // "GetMessage()" from retrieving a message. Exit the application.
                // NOTE: This is a SERIOUS problem and should never happen, so we do
                // not run any WM_CLOSE to cleanly autosave... we will exit quickly.
                throw std::runtime_error("Critical failure in message loop.");
            }
            // NOTE: IsWindow() ensures that the dialog exists, and IsDialogMessage()
            // actually checks if the message belongs to the dialog and if so it processes
            // the message for that dialog! If they both return false, it means it's
            // a "regular (NON-dialog) window" or "thread message" instead, so we use
            // the regular message handlers (translate/dispatch) instead.
            // NOTE: Even though this is a "typical" message loop, it doesn't matter
            // that our application lacks actual (regular) windows, since the DispatchMessage
            // function only calls the callbacks of *registered* windows, and we simply
            // don't have *any* windows! Our dialog is handled by IsDialogMessage().
            else if (!IsWindow(g_hDlg) || !IsDialogMessage(g_hDlg, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        // Use the returned code from the final message (WM_QUIT) as the exit code.
        exitCode = static_cast<int>(msg.wParam);
    }
    catch (const std::exception& ex) {
        // NOTE: Exceptions are never wide (Unicode), so we must use the regular ANSI msgbox.
        MessageBoxA(NULL, ex.what(),
            "Fatal Error", MB_OK);
        exitCode = 1; // Signal error as exit-code.
        goto Exit;
    }

Exit:
    // Exit-cleanup goes here...
    ExitCleanup();

    // Return the exit code to the system.
    return exitCode;
}

void ExitCleanup() noexcept
{
    // NOTE: This function can safely be called multiple times.

    // Destroy and liberate all audio devices and related COM connections.
    g_deviceManager.reset();

    // Remove the notification area icon (if one is registered). Otherwise it lingers after exit.
    if (g_hasNotifyIcon) {
        Shell_NotifyIcon(NIM_DELETE, &g_notifyIconData);
        g_hasNotifyIcon = false;
    }

    // Free the memory used by the dynamically loaded/scaled application (and notification area) icons.
    // NOTE: We free (from system DLL heap?) memory allocated by LoadIconMetric.
    if (g_iconLargeMain != NULL) { DestroyIcon(g_iconLargeMain); g_iconLargeMain = NULL; }
    if (g_iconSmallMain != NULL) { DestroyIcon(g_iconSmallMain); g_iconSmallMain = NULL; }
    if (g_iconLargeDisabled != NULL) { DestroyIcon(g_iconLargeDisabled); g_iconLargeDisabled = NULL; }
    if (g_iconSmallDisabled != NULL) { DestroyIcon(g_iconSmallDisabled); g_iconSmallDisabled = NULL; }

    // Free the memory used by the context-menu.
    // NOTE: This is necessary because Windows only auto-releases menus at exit if the menus are owned
    // by an app-window. And our menu is a popup-menu, so I am unsure whether it's owned by the dialog.
    if (g_hTrayMenu != NULL) {
        DestroyMenu(g_hTrayMenu);
        g_hTrayMenu = NULL;
    }
}

void Dlg_ShowAndForeground() noexcept
{
    // Make window visible again if it's invisible.
    if (!IsWindowVisible(g_hDlg)) {
        ShowWindow(g_hDlg, SW_SHOW);
    }

    // Ensure that the window is no longer minimized.
    if (IsIconic(g_hDlg)) { // "Iconic" means minimized.
        ShowWindow(g_hDlg, SW_RESTORE); // "Restore the minimized window".
    }

    // Attempt to bring the window to the foreground.
    // NOTE: System restricts this function and only performs the action if our calling
    // thread received the most recent Windows input/message event, or similar privileges.
    // NOTE: If the window is on another virtual desktop (Windows 10+), this call will
    // cause that exact virtual desktop to be focused when the app goes to the foreground.
    SetForegroundWindow(g_hDlg);
}

ptrdiff_t Dlg_GetDropdownSelection(
    int dlgItem) noexcept
{
    // Retrieve the item selection offset from the desired list.
    ptrdiff_t selectionIdx = SendDlgItemMessage(g_hDlg, dlgItem, CB_GETCURSEL, 0, 0);

    // If the checkbox lacks a selection, return -1 instead.
    if (selectionIdx == CB_ERR) {
        selectionIdx = -1;
    }

    return selectionIdx;
}

void Dlg_ShowLinkState() noexcept
{
    auto isLinked = g_deviceManager && g_deviceManager->isLinkActive();

    // Apply dialog icon (the top left corner icon).
    // NOTE: We don't need to check if any of the icons are NULL (not loaded),
    // because in that case WM_SETICON simply removes the targeted icon.
    // NOTE: By default, a dialog uses the application's embedded "main" icon
    // as its taskbar icon. But as soon as we send either ICON_SMALL or ICON_BIG,
    // it replaces the taskbar icon too! And we must send both, otherwise it will
    // scale up the ICON_SMALL as a blurry taskbar icon! Also note that if we ever
    // send a NULL (0) icon, the taskbar reverts to the app's embedded icon again.
    SendMessage(g_hDlg, WM_SETICON, ICON_SMALL, (LPARAM)(isLinked ? g_iconSmallMain : g_iconSmallDisabled));
    SendMessage(g_hDlg, WM_SETICON, ICON_BIG, (LPARAM)(isLinked ? g_iconLargeMain : g_iconLargeDisabled));

    // Also update the notification area icon.
    // NOTE: According to various researchers on the internet, the notification tray
    // always makes its own private copy of the icons we give it, which is nice.
    if (g_hasNotifyIcon) {
        g_notifyIconData.hIcon = (isLinked ? g_iconSmallMain : g_iconSmallDisabled);
        Shell_NotifyIcon(NIM_MODIFY, &g_notifyIconData);
    }

    // Update buttons and volume controls based on link-state.
    SendDlgItemMessage(g_hDlg, IDC_BUTTON_LINK, WM_SETTEXT, 0, (LPARAM)(isLinked ? L"Unlink Devices" : L"Link Devices"));
    EnableWindow(GetDlgItem(g_hDlg, IDC_SLIDER_VOLUME), isLinked ? TRUE : FALSE);
    EnableWindow(GetDlgItem(g_hDlg, IDC_CHECK_MUTE), isLinked ? TRUE : FALSE);
    if (!isLinked) {
        // Unlinked: Uncheck the "mute" checkbox and put the volume slider all the way to the left.
        SendDlgItemMessage(g_hDlg, IDC_CHECK_MUTE, BM_SETCHECK, BST_UNCHECKED, 0);
        SendDlgItemMessage(g_hDlg, IDC_SLIDER_VOLUME, TBM_SETPOS, TRUE, 0);
    }
}

void Dlg_LinkDevices(
    bool showErrors)
{
    // Retrieve the item selection offsets from both lists.
    auto masterIdx = Dlg_GetDropdownSelection(IDC_MASTERLIST);
    auto slaveIdx = Dlg_GetDropdownSelection(IDC_SLAVELIST);

    // If any of the checkboxes lack a selection, just unlink any existing connection.
    if (masterIdx < 0 || slaveIdx < 0) {
        Dlg_UnlinkDevices();
        return;
    }

    try {
        // Attempt to link the devices. Throws if there are any problems establishing the link.
        if (g_deviceManager) {
            g_deviceManager->linkDevices(masterIdx, slaveIdx);
        }
    }
    catch (const std::exception& ex) {
        // NOTE: Exceptions are never wide (Unicode), so we must use the regular ANSI msgbox.
        if (showErrors) {
            MessageBoxA(g_hDlg, ex.what(),
                "Link Failed", MB_OK);
        }
    }

    // Show the result of the linking attempt.
    Dlg_ShowLinkState();
}

void Dlg_UnlinkDevices() noexcept
{
    if (g_deviceManager && g_deviceManager->isLinkActive()) {
        g_deviceManager->unlinkDevices();
    }
    Dlg_ShowLinkState();
}

bool Dlg_SaveSettings() noexcept
{
    // Ensure that this function only runs when there's still a dialog.
    if (g_hDlg == NULL) {
        return false;
    }

    // Only save settings to registry if they've been marked as "user has modified manually".
    // NOTE: The purpose is to avoid ultra annoying behavior where the user may open the app
    // while their soundcard device is missing somehow, and then closing the application and
    // fixing their soundcard, and finally re-opening the app but then seeing that their old
    // device choice has now been cleared (forgotten) since the settings had been re-saved while
    // the device was missing. So, instead of being annoying like that, we will simply NEVER
    // save settings UNLESS the user has MANUALLY clicked on the Link/Unlink button OR changed
    // active selection in any of the dropdowns. Thanks to this safeguard, the user can relax
    // and close and re-open the application and be sure that it WILL always re-open with
    // their LAST USER-CONFIGURATION, rather than some annoying auto-saved "broken" state!
    if (g_saveChanges) {
        // Save last-used settings to registry (while ensuring no uncaught exceptions escape).
        try {
            // Prepare initial states for the variables we'll be saving to the registry.
            auto linkActive = g_deviceManager->isLinkActive();
            wstring masterDeviceId;
            wstring slaveDeviceId;

            // Retrieve the item selection offsets from both lists.
            auto masterIdx = Dlg_GetDropdownSelection(IDC_MASTERLIST);
            auto slaveIdx = Dlg_GetDropdownSelection(IDC_SLAVELIST);

            // Retrieve the selected devices and their hardware IDs (throws if the indices are invalid, negative, etc).
            // NOTE: If any of the devices have problems, we'll save an empty ID and no "link active" state.
            try {
                auto masterDevice = g_deviceManager->getDevice(masterIdx);
                masterDeviceId = masterDevice.getId();
            }
            catch (...) {
                linkActive = false;
                masterDeviceId = L"";
            }
            try {
                auto slaveDevice = g_deviceManager->getDevice(slaveIdx);
                slaveDeviceId = slaveDevice.getId();
            }
            catch (...) {
                linkActive = false;
                slaveDeviceId = L"";
            }

            // Open the program's key (creates it if missing).
            // NOTE: We never need elevated privileges to read/write the user's registry,
            // so we'll just silently ignore any throws here (which should never happen).
            winreg::RegKey key{ g_regHKey, g_regSoftwareKey, g_regDesiredAccess };

            // Write the settings to the registry. (Can throw but should never happen.)
            key.SetStringValue(g_regMasterDevice, masterDeviceId);
            key.SetStringValue(g_regSlaveDevice, slaveDeviceId);
            key.SetDwordValue(g_regLinkActive, linkActive ? 1 : 0);

            // Mark the fact that we've successfully saved the changes.
            g_saveChanges = false;

            return true;
        }
        catch (...) {}
    }

    return false;
}

BOOL CALLBACK AboutDlgProc(
    HWND hDlg,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    if (message == WM_COMMAND) {
        auto controlId = LOWORD(wParam); // Control's identifier.
        switch (controlId)
        {
        case IDOK: // User pressed the "OK" button in the dialog.
        case IDCANCEL: // Triggered automatically by methods such as user pressing escape.
            EndDialog(hDlg, 0);
            return TRUE;
        }
    }

    return FALSE; // Let default handler take care of all other events.
}

BOOL CALLBACK QuitDlgProc(
    HWND hDlg,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    if (message == WM_COMMAND) {
        auto controlId = LOWORD(wParam); // Control's identifier.
        switch (controlId)
        {
        case IDC_BUTTON_QUIT:
        case IDC_BUTTON_MINIMIZE:
        case IDC_BUTTON_CANCEL:
            EndDialog(hDlg, controlId); // Return ID of selected control.
            return TRUE;
        case IDCANCEL: // Triggered automatically by methods such as user pressing escape.
            EndDialog(hDlg, IDC_BUTTON_CANCEL); // Return "cancel".
            return TRUE;
        }
    }

    return FALSE; // Let default handler take care of all other events.
}

BOOL CALLBACK MainDlgProc(
    HWND hDlg,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
    case WM_DPICHANGED: // Win 8.1+ only: The DPI has changed (either by user, or by dragging to different-DPI screen).
    {
        // NOTE: High-DPI is *automatically* handled thanks to "PerMonitorV2" mode
        // (see "DPIAware.manifest") on Win10, and the system does this regardless of
        // whether we return TRUE or FALSE here, which means that it's integrated
        // into the Win10+ OS and not part of the default dialog handler!
        // We don't have to do anything here. But this code block is here just
        // to note that older OS versions from Windows 8.1 and onwards send this
        // event, BUT that we HAVEN'T implemented any high-DPI intelligence
        // WHATSOEVER for Windows 7/8/8.1! See "DPIAware.manifest" for why.
        // The only reason why we're marking the application as high-DPI aware
        // even on those older systems (despite lacking manual scaling code) is
        // to ensure that all of our screen coordinates become non-virtualized,
        // so that we can display popup menus at correct screen positions, etc,
        // regardless of OS. Read the manifest for more information.
        return FALSE; // Say that we didn't handle this event. Use default handler.
    }

    case WM_SYSCOMMAND: // User requested a command from the Window-menu (via taskbar/window icon), titlebar buttons (ie. min/max/close), or Alt-F4.
    {
        if (wParam == SC_CLOSE) { // User wants to close the window.
            // Alright, we know that the user has manually requested to close the window,
            // but we unfortunately don't know if they've used Alt-F4 or not. There's
            // a way to check if they used a "mnemonic" (Alt-F4), but that also triggers
            // for the taskbar-rightclick "Close window" option, so we can't rely on it.

            // Either way, we must now show a confirmation dialog to be sure that the user
            // knows what they're doing.
            auto choice = DialogBox(g_hInstance,
                MAKEINTRESOURCE(DLG_QUITPANEL), hDlg, (DLGPROC)QuitDlgProc);
            switch (choice)
            {
            case IDC_BUTTON_QUIT:
                return FALSE; // Use default "quit" handler (exit program).
            case IDC_BUTTON_MINIMIZE:
                ShowWindow(hDlg, SW_MINIMIZE); // Minimize the main dialog.
                // Also fall through to "default" to abort quitting...
            default: // IDC_BUTTON_CANCEL
                return TRUE; // Abort the quitting.
            }
        }

        return FALSE; // Let Windows handle most of the syscommands.
    }

    case APP_WM_ICONNOTIFY: // Messages from notification icon interaction.
    {
        auto eventType = LOWORD(lParam);
        auto iconId = HIWORD(lParam);

        switch (eventType)
        {
            //case WM_LBUTTONDBLCLK: // Left double-click. (Pointless; NIN_SELECT is always sent with the first click during double-click).
        case NIN_SELECT: // Left click-up. Also: "user selects icon with mouse activates it with ENTER" (the latter is no longer true in Win10, which sends NO EVENT if you hover with the mouse and then use the keyboard to "activate").
        case NIN_KEYSELECT: // User "activated" icon with ENTER/SPACEBAR (in "Win+B" keyboard mode). Sent TWICE if ENTER is used.
        {
            // Ensure that the window is visible, not minimized, and bring it to front.
            Dlg_ShowAndForeground();

            break;
        }

        case WM_CONTEXTMENU: // User requests tray icon menu (ie. via right-click or requesting menu while icon has keyboard-focus (via Shift-F10 or dedicated "menu"-key)).
        {
            // Get the X and Y mouse cursor positions; or, if menu was opened via keyboard,
            // it is the tray icon's position. The coordinates are real pixels (DPI aware).
            auto pos = MAKEPOINTS(wParam); // Menu coordinates.

            // Load the notification tray icon's context-menu (if not loaded yet).
            if (g_hTrayMenu == NULL) {
                g_hTrayMenu = LoadMenu(g_hInstance, MAKEINTRESOURCE(IDR_MENU_TRAY));
            }

            // Check if the menu is loaded, and display the sub-menu containing the popup items.
            // NOTE: The sub-menu does not need manual resource releasing since it's part of the bigger menu.
            if (g_hTrayMenu != NULL) {
                auto trayMenu = GetSubMenu(g_hTrayMenu, 0);
                if (trayMenu != NULL) {
                    // Fetch the optimal horizontal placement of the menu (to the left or right of cursor).
                    // NOTE: "Nonzero if drop-down menus are right-aligned with their corresponding menu-bar item;
                    // or 0 if the menus are left-aligned." (MSDN).
                    // NOTE: We're using the non-aware (DPI unaware) version of the API, because "GetSystemMetricsForDpi"
                    // is only available in Windows 10+. Still, it seems like we're only asking for a RTL/LTR text direction
                    // flag, most likely based on the user's language (ie. Arabic computers are RTL), so DPI shouldn't matter!
                    auto menuHorizAlign = GetSystemMetrics(SM_MENUDROPALIGNMENT) == 0 ? TPM_LEFTALIGN : TPM_RIGHTALIGN;

                    // We must first set the parent window as the "foreground" window, otherwise the popup menu won't
                    // close at all if you click outside the menu. This fact is documented in "TrackPopupMenuEx" (MSDN).
                    // NOTE: This works properly even if the dialog is in SW_HIDE and Minimized state. We DON'T need
                    // to make it visible. The call below is enough to make hDlg "foreground" anyway, and fixes the menu.
                    // NOTE: Most likely, windows are able to have the "foreground" flag regardless of WHERE they are,
                    // and the OS only clears their flag when you click/tab to something else. And the popup menu probably
                    // listens to some "parent's foreground focus lost" event to detect when to auto-close the menu.
                    SetForegroundWindow(hDlg);

                    // Display the menu. This operation is BLOCKING and won't return until the menu is closed!
                    // NOTE: We use the window-message mode which means the selected item is sent to OUR message queue.
                    // NOTE: Despite this call being blocking, it thankfully doesn't prevent the volume callback
                    // from directly updating the volume-slider and mute-checkbox whenever the device volume changes!
                    // NOTE: We do NOT specify vertical alignment (ie. TPM_BOTTOMALIGN). By not forcing an alignment,
                    // the menu will be vertically auto-aligned by Windows, based on where the icon is on the screen.
                    TrackPopupMenuEx(trayMenu,
                        menuHorizAlign | TPM_RIGHTBUTTON /* allow both left and right clicks */,
                        pos.x, pos.y, hDlg, NULL);
                }
            }

            break;
        }
        }

        return TRUE;
    }

    case WM_SIZE: // Fired AFTER the window's size/state has changed (ie. after animation has finished).
    {
        switch (wParam)
        {
        case SIZE_MINIMIZED: // Window has become minimized.
            // Make window invisible to hide it from the taskbar while it's minimized.
            // NOTE: The controls (ie. volume/mute) will continue to update while the dialog is hidden.
            ShowWindow(hDlg, SW_HIDE);
            return TRUE;
        }

        break;
    }

    case APP_WM_BRINGTOFRONT: // Used by other instances to tell this instance to activate itself.
    {
        // Ensure that the window is visible, not minimized, and bring it to front.
        Dlg_ShowAndForeground();

        return TRUE;
    }

    case WM_INITDIALOG: // Executed by CreateDialog, for initializing the dialog box controls.
    {
        // Save program-global handle for our dialog.
        g_hDlg = hDlg;

        // Register the notification area icon.
        // NOTE: We must do this BEFORE we try to NIM_MODIFY, since we need to be able to set the
        // correct link-status icon during our initialization further down. But we also can't NIM_ADD
        // before CreateDialog, since we don't know our window handle until we've been created.
        g_notifyIconData.cbSize = sizeof(g_notifyIconData); // NOTE: Since we only target Win7+, we don't need "NOTIFYICONDATA_V3_SIZE" (or older) here.
        g_notifyIconData.uVersion = NOTIFYICON_VERSION_4; // Use "modern" tray icons introduced in Vista and later. Improves and adds modern event messages!
        g_notifyIconData.hWnd = hDlg; // Send all "tray icon" window messages to our dialog's message handler.
        g_notifyIconData.uID = 1; // We use ID-based icons since GUID icons are messy (tied to the executable path unless app is Authenticode-signed).
        g_notifyIconData.uFlags = NIF_ICON | NIF_TIP | NIF_SHOWTIP | NIF_MESSAGE; // Setting desired features for the tray icon.
        g_notifyIconData.uCallbackMessage = APP_WM_ICONNOTIFY; // Which window message the icon sends to the hWnd.
        StringCchCopy(g_notifyIconData.szTip, ARRAYSIZE(g_notifyIconData.szTip), L"Volume Linker"); // Tooltip. Can be up to 128 chars in current structure. Safest way of writing the string (https://docs.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-notifyicondataa).
        g_notifyIconData.hIcon = g_iconSmallDisabled; // Begin in disabled state. Use the globally loaded icon as our notification icon.
        if (Shell_NotifyIcon(NIM_ADD, &g_notifyIconData)) { // Add the icon to the tray. Returns TRUE on success.
            Shell_NotifyIcon(NIM_SETVERSION, &g_notifyIconData); // Tell icon to behave according to the "uVersion" value.
            g_hasNotifyIcon = true;
        }

        // Apply the correct (32-bit or 64-bit) dialog title.
#ifdef _WIN64
        SetWindowText(hDlg, APP_WINDOW_TITLE_64);
#else
        SetWindowText(hDlg, APP_WINDOW_TITLE_32);
#endif

        // Set the min/max value range of the volume slider.
        SendDlgItemMessage(hDlg, IDC_SLIDER_VOLUME, TBM_SETRANGEMIN, FALSE, 0);
        SendDlgItemMessage(hDlg, IDC_SLIDER_VOLUME, TBM_SETRANGEMAX, FALSE, MAX_VOL);

        // Tell the device manager to use (auto-update) our dialog and its volume-controls.
        g_deviceManager->setDialog(hDlg, IDC_CHECK_MUTE, IDC_SLIDER_VOLUME);

        // Read last-used settings from registry (while ensuring no uncaught exceptions escape).
        bool linkActive = false;
        wstring masterDeviceId;
        wstring slaveDeviceId;
        try {
            // Open the program's key (creates it if missing).
            // NOTE: We never need elevated privileges to read/write the user's registry,
            // so we'll just silently ignore any throws here (which should never happen).
            winreg::RegKey key{ g_regHKey, g_regSoftwareKey, g_regDesiredAccess };

            // Attempt to read all settings. Will throw if types mismatch or values are missing.
            linkActive = (key.GetDwordValue(g_regLinkActive) == 1);
            masterDeviceId = key.GetStringValue(g_regMasterDevice);
            slaveDeviceId = key.GetStringValue(g_regSlaveDevice);
        }
        catch (...) {
            // If any of the settings couldn't be read, we'll just set them all to defaults.
            linkActive = false;
            masterDeviceId = L"";
            slaveDeviceId = L"";
        }

        // Retrieve vector of all detected audio playback devices.
        auto audioDevices = g_deviceManager->getAudioDevices();

        // Populate the dropdown menus, and detect which entries (if any) should be auto-selected.
        ptrdiff_t counter = 0;
        ptrdiff_t masterIdx = -1;
        ptrdiff_t slaveIdx = -1;
        for (auto& device : audioDevices) {
            SendDlgItemMessage(hDlg, IDC_MASTERLIST, CB_ADDSTRING, 0, (LPARAM)device.getNameMS());
            SendDlgItemMessage(hDlg, IDC_SLAVELIST, CB_ADDSTRING, 0, (LPARAM)device.getNameMS());

            auto deviceId = device.getId();
            if (deviceId.compare(masterDeviceId) == 0 && masterDeviceId.length() > 0) {
                masterIdx = counter;
            }
            if (deviceId.compare(slaveDeviceId) == 0 && slaveDeviceId.length() > 0) {
                slaveIdx = counter;
            }

            ++counter;
        }

        // Force the dropdown menus to select their last-used devices (if invalid index = clears selection).
        if (masterIdx != -1) {
            SendDlgItemMessage(hDlg, IDC_MASTERLIST, CB_SETCURSEL, (WPARAM)masterIdx, 0);
        }
        if (slaveIdx != -1) {
            SendDlgItemMessage(hDlg, IDC_SLAVELIST, CB_SETCURSEL, (WPARAM)slaveIdx, 0);
        }

        // Automatically link again if both devices were found, and were linked last time (OR told to force-link).
        // NOTE: We always suppress error popup boxes here, because it would be extremely annoying to have this
        // program on autostart (as most people would), and to get a popup error during login. It's better that
        // people just personally realize volume isn't linked (if there was a problem) and open the GUI to fix it.
        if ((linkActive || g_optForceLink) && masterIdx >= 0 && slaveIdx >= 0) {
            Dlg_LinkDevices(false); // No error boxes on failure.
        }
        else {
            // Since we didn't auto-link, we should at least set the control states to the unlinked state.
            Dlg_ShowLinkState();
        }

        return TRUE;
    }

    case WM_QUERYENDSESSION: // Windows is shutting down and wants to know if we're ok with that.
        // SUPER IMPORTANT: The normal shutdown flow is that the OS asks about WM_QUERYENDSESSION,
        // and you return TRUE if you are ready to shut down. Next, the OS sends WM_ENDSESSION
        // and lets you take up to 5 seconds to finish saving files before you return a value,
        // and after that it kills your application instantly without calling any "other" clean
        // shutdown/WM_QUIT/DestroyWindow etc. The way Microsoft's "Raymond Chen" puts it is that
        // it basically "tears down the whole building" and just destroys the resource memory
        // immediately without worrying about housekeeping first. In short, "WM_ENDSESSION" *is*
        // your app's "exit point"! Furthermore, the docs say that you can OPTIONALLY choose
        // to manually destroy your window and post quit messages during the WM_ENDSESSION
        // event, "but you aren't required to do so"... But that's POINTLESS, because the app is
        // always KILLED after WM_ENDSESSION and is NOT told to exit its message-loop (even if
        // you manually post WM_QUIT), and is NOT given a chance to cleanly exit from its "main()"
        // routine. So ALL shutdown code MUST be handled WITHIN the WM_ENDSESSION call... In fact,
        // in Raymond Chen's blog post, he literally says "If you need to do cleanup, you need
        // to do that BEFORE returning from WM_ENDSESSION, because you cannot rely on ANY further
        // code in your program running after you've returned", and he further clarifies that
        // NOT EVEN C++ OBJECT DESTRUCTORS WILL RUN. The application is literally TERMINATED.
        //
        // But that's actually not every ceaveat, because there's a serious, undocumented side-effect
        // if you DO decide to manually DestroyWindow during your WM_ENDSESSION handler... If you
        // destroy the window, the application INSTANTLY TERMINATES upon that call, and does NOT
        // run ANY more lines of code after that DestroyWindow call! It took Mozilla 2.5 WEEKS
        // to discover this undocumented side-effect: https://bugzilla.mozilla.org/show_bug.cgi?id=333907
        //
        // Furthermore, doing PostQuitMessage is completely pointless since your application is
        // terminated before the loop ever has a chance to process that message. So don't waste
        // precious time posting that message either!
        //
        // So, the FINAL summary becomes: Treat WM_ENDSESSION as your exit point, and do NOT destroy
        // windows or post WM_QUIT from there. Just cleanly save work and delete any objects whose
        // destructors MUST run, and remember that you only have 5 seconds of real time to do
        // all of that work and MUST return from the handler before that. And then, let Windows
        // "tear down the building" (forcefully terminates your program). ;-)
        //
        // However, all of this is harder to implement due to the fact that OUR window is Dialog-
        // based and has a DEFAULT handler. The problem is that if we return FALSE (int 0), the
        // default handler runs instead. But the OS *wants* us to return 0 from WM_ENDSESSION if
        // we've successfully and gracefully prepared ourselves to be killed. But doing so will
        // just trigger the dialog's DEFAULT handler instead, which returns a non-0 value to the
        // OS in that situation, and makes the OS think we're "not responding" during shutdown.
        //
        // Here are the results based on what we return:
        // * QUERYEND: TRUE, ENDSESSION: FALSE (aka 0) = We enter a weird state where the OS
        // believes that we've stopped responding (because the default dialog handler gets
        // confused by QUERYEND: TRUE by us).
        // * QUERYEND: FALSE, ENDSESSION: FALSE = Default dialog handler runs for both events,
        // and NO other events are fired (no WM_CLOSE any other nice stuff, not even IDCANCEL).
        //
        // Therefore, the ONLY choice is to return FALSE for BOTH, so that the default dialog
        // handler's "clean shutdown" executes for both events. However, we still want to run
        // our own clean shutdown handling. Luckily, the window-message flow is STILL always
        // "windows OS -> our callback -> the default handler if our callback said FALSE". So we
        // can ALWAYS see BOTH messages (and can take some time, doing cleanup BEFORE we return
        // FALSE to let the default dialog handler run, and THEN letting Windows insta-kill us).
        //
        // So that's our solution. We'll return FALSE in both cases, and we'll handle the
        // WM_ENDSESSION message with the "shutting down" flag by doing an auto-save in there,
        // as well as manually deleting any objects whose destructors MUST run. And we will
        // NEVER, EVER call "DestroyWindow" or "PostQuitMessage" during the cleanup steps! ;-)
        //
        // ALSO NOTE: If our secondary DLG_QUITPANEL is visible, it's always parented to our
        // main dialog (it's modal), so what happens is that the DLG_QUITPANEL auto-closes FIRST
        // via its own default dialog handler, and THEN our main dialog runs its own final
        // cleanup. In other words, DLG_QUITPANEL WON'T interfere with our final auto-save/cleanup!

        return FALSE;

    case WM_ENDSESSION: // Windows is informing us whether the session is truly ending or not.
    {
        // We must now handle the WM_ENDSESSION(wParam = TRUE) state to do a clean application
        // shutdown without data loss. There's no guarantee how long the process will live after
        // we return from handling this message (usually it's INSTANTLY TERMINATED without even
        // running any object destructors), but what we do know is that the OS gives us 5 seconds
        // to handle this WM_ENDSESSION message:
        // https://docs.microsoft.com/en-us/previous-versions/windows/desktop/ms700677(v=vs.85)
        // And whatever we do, we MUST avoid calling DestroyWindow (and PostQuitMessage).

        // NOTE: This shutdown routine has been confirmed to work properly (destroy all critical
        // objects, auto-save) on Win7 SP1, Win8.1 and Win10. Win8 was not tested but assumed ok.

        if (wParam == TRUE) { // Session is truly ending (shutdown hasn't been aborted).
            // Save configuration to registry.
            Dlg_SaveSettings();

            // Break any active link/callback, just to be nice and clean...
            Dlg_UnlinkDevices();

            // Because no other code will run after we return (not even destructors or the
            // remainder of "main()"), we must manually clean up some critical destructors.
            // We do that by running the same cleanup function as "main()" would have done.
            ExitCleanup();

            // Also uninitialize the COM connection for this thread (if any still exists).
            // NOTE: During NORMAL app closing, this is handled by the object named "cleanup".
            CoUninitialize();

            // NOTE: As mentioned, our application will now be TERMINATED after this returns!
        }

        return FALSE;
    }

    case WM_CLOSE: // We've been told to close ourselves via "standard" methods such as the X button.
    {
        // Save configuration to registry.
        Dlg_SaveSettings();

        // Break any active link/callback, just to be nice and clean...
        Dlg_UnlinkDevices();

        // Close and unload the window (dialog) resources.
        if (g_hDlg != NULL) {
            DestroyWindow(g_hDlg); // Immediately causes a WM_DESTROY to be processed (before returning).
            g_hDlg = NULL; // Should have been set automatically by WM_DESTROY, but let's be extra sure!
        }

        // Tell the main application message loop to quit (by posting a WM_QUIT message).
        // NOTE: We'll base our exit code on the device manager's result. In almost all cases,
        // it will be 0 (no error). However, if the volume-linking callback failed critically
        // while syncing volumes, it will have set itself to a non-zero exit code and then told
        // this dialog to WM_CLOSE. So we'll exit with the code from the device manager!
        int exitCode = g_deviceManager ? g_deviceManager->getExitCode() : 0;
        PostQuitMessage(exitCode);

        return TRUE;
    }

    case WM_DESTROY:
    {
        // Mark the fact that the window has been destroyed (unloaded).
        g_hDlg = NULL;

        return TRUE;
    }

    case WM_COMMAND:
    {
        // Parse the message. These parsing rules are true for all except 1 or 2 notification types.
        auto notificationCode = HIWORD(wParam); // Notification code.
        auto controlId = LOWORD(wParam); // Control's identifier.
        auto hControl = (HWND)lParam; // Control's window handle.

        switch (notificationCode)
        {
        case BN_CLICKED: // Button clicked (even if done via keyboard-spacebar).
            // We must check the control, since this event triggers for "X" (close) clicks too.
            switch (controlId)
            {
            case IDC_BUTTON_LINK:
            {
                // Automatically toggle between linking or un-linking.
                if (!g_deviceManager->isLinkActive()) {
                    Dlg_LinkDevices(true);
                }
                else {
                    Dlg_UnlinkDevices();
                }

                // Mark the fact that the user has MANUALLY changed the device/link-settings.
                // NOTE: We don't care whether the link succeeded or not; the mere fact that
                // the user has clicked the button is enough for us to mark changes for saving.
#ifdef _DEBUG
                if (!g_saveChanges) {
                    OutputDebugStringA("> Settings marked for saving (by link-button).\r\n");
                }
#endif
                g_saveChanges = true;

                return TRUE;
            }

            case IDC_CHECK_MUTE:
            {
                // Update the master device's mute state (will also sync to the slave device automatically).
                auto nChecked = SendDlgItemMessage(hDlg, IDC_CHECK_MUTE, BM_GETCHECK, 0, 0);
                BOOL bMuted = (BST_CHECKED == nChecked);
                g_deviceManager->setMasterMute(bMuted);
                return TRUE;
            }
            }

            break;

        case CBN_SELCHANGE: // Combobox selection changed (won't trigger when just opening and closing list without changing).
        {
            // When the user changes device selection, automatically unlink any active link.
            Dlg_UnlinkDevices();

            // Mark the fact that the user has MANUALLY changed the device/link-settings.
#ifdef _DEBUG
            if (!g_saveChanges) {
                OutputDebugStringA("> Settings marked for saving (by dropdown).\r\n");
            }
#endif
            g_saveChanges = true;

            return TRUE;
        }
        }

        switch (controlId)
        {
        case IDOK: // User pressed "enter" in the dialog.
        case IDCANCEL: // User pressed "escape" in the dialog, *or* the DEFAULT "WM_CLOSE" handler ran (which then sends this event too).
            // NOTE: Since our dialog is acting as the main application window, we want to suppress
            // both kinds of "enter/escape" key presses in the dialog. We already override the default
            // handler for WM_CLOSE, but a direct "escape" press still sends this event, so we're
            // just ensuring that absolutely NO action happens when these keys are pressed.
            return TRUE; // Do nothing.

        case IDM_TRAYMENU_SHOW:
            // Ensure that the window is visible, not minimized, and bring it to front.
            Dlg_ShowAndForeground();

            return TRUE;

        case IDM_TRAYMENU_ABOUT:
            // Display the about-box in a modal (blocking) way.
            DialogBox(g_hInstance,
                MAKEINTRESOURCE(DLG_ABOUT), hDlg, (DLGPROC)AboutDlgProc);

            return TRUE;

        case IDM_TRAYMENU_QUIT:
            // Since they've right-clicked the notification icon and selected "Quit",
            // we'll assume that they know what they are doing, and we won't ask them
            // if they want to "Quit, Minimize or Cancel". We'll send a direct WM_CLOSE!
            PostMessage(hDlg, WM_CLOSE, 0, 0);

            return TRUE;
        }

        break;
    }

    case WM_HSCROLL: // An event has happened in a horizontal scrollbar.
    {
        // Only proceed if the event was sent by a scrollbar control (lParam is non-NULL),
        // and react to all scrollbar movement events except "movement has ended".
        // Docs: https://docs.microsoft.com/en-us/windows/win32/controls/wm-hscroll
        if (lParam != NULL && LOWORD(wParam) != SB_ENDSCROLL) {
            // Retrieve scrollbar position (only whole integers, from 0 to 100 (MAX_VOL)).
            ptrdiff_t iVolume = SendDlgItemMessage(hDlg, IDC_SLIDER_VOLUME, TBM_GETPOS, 0, 0);

            // Ensure valid numeric range, as an extra "failsafe" protection just in case...
            if (iVolume < 0) { iVolume = 0; }
            else if (iVolume > MAX_VOL) { iVolume = MAX_VOL; }

            // Convert the volume to a float (range: 0.0 to 1.0) and set the device volume.
            float fVolume = static_cast<float>(iVolume) / MAX_VOL;
            g_deviceManager->setMasterVolume(fVolume);

            // The standard Windows system controls for volume (keyboard keys or volume mixer)
            // dynamically manage the "mute" state based on the user's volume actions, as follows:
            // - When volume reaches 0, the audio device is toggled to "muted".
            // - When volume is moved to any non-zero position, it is always unmuted (if muted).
            // - The latter applies even if muted at volume 60 and then moved to 61 (unmutes).
            // We need to replicate the same behavior here since the API won't do it automatically.
            if (iVolume == 0) {
                // Mute the master device (will also sync to the slave device automatically).
                g_deviceManager->setMasterMute(TRUE);
                SendDlgItemMessage(hDlg, IDC_CHECK_MUTE, BM_SETCHECK, BST_CHECKED, 0);
            }
            else {
                // Optimize by only sending "unmute" if it was actually muted. We'll use our "mute"
                // checkbox's state to determine this, since it's perfectly synced to the system's
                // actual mute-state by the volume-callback (and when GUI-user toggles it manually).
                auto nChecked = SendDlgItemMessage(hDlg, IDC_CHECK_MUTE, BM_GETCHECK, 0, 0);
                BOOL bMuted = (BST_CHECKED == nChecked);
                if (bMuted) {
                    // Unmute the master device (will also sync to the slave device automatically).
                    g_deviceManager->setMasterMute(FALSE);
                    SendDlgItemMessage(hDlg, IDC_CHECK_MUTE, BM_SETCHECK, BST_UNCHECKED, 0);
                }
            }

            return TRUE;
        }

        break;
    }
    }

    // Signal that we didn't handle the event.
    return FALSE;
}
