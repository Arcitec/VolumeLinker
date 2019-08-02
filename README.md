# Volume Linker by VideoPlayerCode

![Volume Linker Screenshot](https://raw.githubusercontent.com/VideoPlayerCode/VolumeLinker/master/Screenshot.png)

A minimalistic and robust application for linking/synchronizing the volume
between two audio interfaces in Windows.

Potential uses include audio signal processing and routing setups.

For example, you may be routing all system audio to a virtual audio device
(in other words, a virtual cable), and then using an audio DSP/VST host
which fetches the audio from the virtual cable, processes it with some
VST effects to improve the sound quality or apply virtual surround effects
(such as the excellent Waves NX), and then finally outputs the result
to your system's real sound card.

The classic problem with such a configuration is that your volume controls
in Windows won't work anymore, since they will only change the volume
of your system's "default playback device" (which is your "virtual cable"
device in such a configuration), and therefore won't change the volume
of your *hardware* output device. In other words, you'll be stuck with
a static volume amount, and changing the volume becomes very tedious.

That's where this application comes in... It allows you to synchronize
the volume of your real hardware output device to automatically match
the state of your virtual device. So when you change volume in Windows,
and the virtual device's volume changes, your real hardware device
will also change volume automatically and seamlessly!

This application was created to solve that specific problem, but there
may be other use cases for it as well. Have fun!


## Installation

1. Download the [latest release of Volume Linker](https://github.com/VideoPlayerCode/VolumeLinker/releases).

2. Unzip the application into any folder on your hard disk.

3. Start `VolumeLinker32.exe` (32-bit) or `VolumeLinker64.exe` (64-bit).
   There is no difference between the two applications, but if you have
   a 64-bit version of Windows, you should be using the 64-bit version
   since it's optimized for such processors.


## How to Use

1. Select your source audio device in the `Master (sync from)` dropdown.

2. Select your target audio device in the `Slave (sync to)` dropdown.

3. Press the `Link Devices` button. The volume of the master device will
   immediately be copied to the slave device. And any changes to the master
   device's volume will instantly be synchronized to the slave device
   while the devices remain linked.

4. The interface also contains a volume slider and a mute control, which
   automatically shows you the master device's volume, and allows you to
   change the volume by using the slider. This is useful for testing the
   link and making sure that the slave device's volume is also changing.

5. The application *must* continue running to maintain the link. Feel free
   to minimize the application to hide its window. The application will
   sit in the system tray (notification area), and you can always open
   it again by clicking on its tray icon, or by launching the .exe again.


## Automatic Startup at Login

1. You must use a method such as an `Autostart` link or the Windows
   `Task Scheduler` to automatically start the application. If you are
   unfamiliar with how to do this, simply use your favorite web search
   engine and educate yourself.

2. The application supports two command-line switches intended to make
   your automatic startup experience better. They are as follows:
   
   `/minimized` (or `/m` or `/minimize`):
   Volume Linker will start in the "minimized to tray" state, so that
   you won't have to see its window automatically appear on your desktop.
   
   `/link` (or `/l`):
   Always attempt to link devices (if both master and slave are selected)
   at startup, even if they were unlinked while the program was last
   closed. This attempt is done silently (no error popup boxes). If there
   are any problems, the devices will simply remain unlinked.

3. The recommended startup command is `VolumeLinker64.exe /l /m`, which
   will ensure that the application starts minimized, and that it always
   attempts to link the last-selected devices at startup, regardless of
   whether you may have manually unlinked them during a previous session.


## Settings Storage

1. The settings are automatically saved when the application is closed,
   but only if you've *manually* changed any of the settings since opening
   the application (by changing the selections in the master/slave dropdown
   menus, or pressing the link/unlink button).
   
   This behavior is simply intended to automatically protect you against
   inadvertent overwriting of your settings, such as if you open the
   application and discover that your audio device is temporarily missing
   from your system (perhaps you've disabled it) and has therefore become
   de-selected. In that case, just close the application, fix the problem,
   and start the application again. Your old settings will still be active,
   thanks to the save-protection. Users don't even have to think about this
   protection method, since it is automatic, but it is mentioned here since
   some people are sure to be curious about it.

2. Device selections are saved by their internal hardware IDs, which is
   the only correct and robust method to perfectly identify a specific
   audio device (since multiple devices may have the same name).
   
   However, this may not always work properly with USB audio devices, since
   they *possibly* change their hardware ID every time they are connected
   and disconnected. Thankfully, most people use internal or always-connected
   devices with permanent IDs. Check your device if you are having problems.

3. All settings are stored per-user, in the following registry key:
   
   `HKEY_CURRENT_USER\Software\VolumeLinker`
   
   If you want a portable version of this application, and you have OCD
   about cleanliness, then you may want to make a wrapper which exports
   and deletes that registry key on exit, and then re-imports it again
   the next time you start the application. That's your own responsibility,
   however. This application will always be designed to use the registry
   for settings storage, since it is the most byte-safe and reliable storage
   method on Windows.
   
   Furthermore, the settings that are stored are very minimal, and by
   most standards Volume Linker already qualifies as a portable application,
   since it consists of a single re-distributable .exe file which runs on
   any supported system.


## System Requirements and Performance

Supported Operating Systems:

* Windows 10.
* Windows 8.1.
* Windows 7 SP1 (Service Pack 1).

Older operating systems are **not** supported, due to lacking the necessary
system APIs. Future operating systems after Windows 10 *should* work too.

The application has been fully tested on all supported operating systems
above. However, on Windows 8.1 you will see Microsoft's "SmartScreen"
warning you about the lack of a developer certificate, so simply read their
silly warning and then accept it -- and no, there's no way that I'm going
to pay Microsoft money just to distribute this application with a useless
Windows 8 certificate. If you're using Windows 8, you should be familiar
with seeing annoying "SmartScreen" warnings by now! You may *also* get
false (incorrect) detections in Microsoft's Defender for old versions
of Windows; in that case, update your Defender to the latest version,
or just verify that it's clean via the online scanner at virustotal.com.

You may also be happy to know that there's full support for high-DPI
screens, which means that the GUI will always look crisp and beautiful.

As for performance, you can expect the application to use `0% CPU`,
and around `1-2 MB` of RAM. However, if you are doing a ton of rapid
volume slider movements via Windows or the application's GUI (which have
very smooth volume sliders and send a ton of volume change events),
you may get a temporary spike of `0.2-0.8% CPU` during the movement,
before it returns back to `0% CPU` again. Either way, as you can see,
the application is extremely light and won't impact your system at all!

Furthermore, the application contains the *statically embedded* version
of the Microsoft Visual C++ runtime, which means that you won't have
to install any C++ runtime on your system. It - just - works!

That's why the .exe files are quite large, but it's worth it for the
convenience of not needing to install any additional software!


## Notes about Fast User Switching

When you're performing "fast user switching", the other users on your
computer actually *remain logged in*, and their applications continue
to run on your system even while you're using another account.

Therefore, if you are using Volume Linker, and then "fast switching" to
another account, you won't be able to open Volume Linker on the *other*
account, and will see an error message about only being able to run "one
instance of Volume Linker per machine".

This is because it would be extremely dangerous to link more than two
devices with each other. For example, one user may sync device A to
device B, and the other may sync device B to device A, which would
create an infinite loop that would do very bad things to your system.

There are multiple ways to get around this limitation:

* Start Volume Linker on any of your accounts, and then do a "fast user
  switch" to one of your other account(s). The link will remain active,
  and any volume changes will automatically be synced even while you
  are using the other accounts. That should be good enough for most
  people.

* Alternatively, simply close Volume Linker before switching accounts.

* But the best alternative would simply be to just do a regular "Log Out",
  which will close all of the user's applications completely. This means
  that your other users get 100% of your computer's performance, since
  no other accounts remain logged in and wasting CPU. It also means that
  there won't be any conflicts, since Volume Linker won't be running on
  the other accounts anymore. This is the recommended method!


## Software License and Copyright

This software is licensed under the GNU General Public License v3.

Copyright (c) 2019 VideoPlayerCode.

Website:
https://github.com/VideoPlayerCode/VolumeLinker


## Recommended 3rd Party Software

The intention for creating Volume Linker was to make it easy to control
your system's *hardware* volume output via a *virtual* audio device,
when you're processing system audio through special effects.

Therefore, this section simply lists software that I highly recommend
for people who want to do audio processing on their system:

* **Best virtual audio cable**: "Virtual Audio Cable (VAC)" by Evgenii
  Muzychenko, from https://vac.muzychenko.net/en/.
  
  There are many other applications for "virtual cables", but this
  is the highest performing, most accurately programmed of them all.
  The author is a genius of Windows driver programming, who has been
  working on this software since 1998 and knows everything about
  the Windows audio driver APIs, and his application allows complete
  customization of the virtual cables it creates. You should be
  creating cables using the "WaveRT" protocol if you are on Win10,
  since that's the highest-performance, lowest-latency protocol.
  
  Some free or alternative drivers exist (such as LoopBeAudio and
  VB-Cable), but all of them are buggy and will not work properly
  for all cases. It's clear that their authors lack the technical
  skill and driver knowledge that went into making VAC.

* **Best headphone calibration plugin**: "Sonarworks Reference
  for Headphones", from https://www.sonarworks.com/reference/headphones.
  
  All headphones and speakers color the sound, and this plugin is
  used by music professionals to calibrate the frequency profile
  of your headphones/speakers, to balance the sound for perfect
  tonal accuracy. It is the industry standard for this purpose.

* **Best crossfeed processing plugin**: "CanOpener Studio" by
  Goodhertz, from https://goodhertz.co/canopener-studio.
  
  When you are listening to stereo music on headphones, your ears
  will get a very unnatural and fatiguing stereo image, since each
  ear gets an isolated signal, with zero ambience or feeling. This
  can quickly lead to claustrophobia and fatigue. Music professionals
  therefore use applications such as the excellent CanOpener Studio,
  to "open up" the headphones by intelligently blending the left
  and right signals, which creates a very natural and airy sound
  without changing the audio integrity whatsoever. This is perfect
  even for the most demanding audiophiles.

* **Best virtual surround plugin**: "Waves NX" by Waves,
  from https://www.waves.com/plugins/nx.
  
  It is actually possible to create virtual surround sound by
  applying binaural HRTF (head-related transfer function) processing
  to a signal, and calculating the filtering that your ears and head
  would have done to provide "locational clues" in the real world.
  
  The Waves NX plugin is without a doubt the industry leader when
  it comes to transparent sound and realism. They are used by
  music producers to listen to their mixes while working, and
  is therefore tuned for transparency and realism. It's like
  listening to your audio in a really high-quality studio with
  perfect acoustics. And the plugin lets you tweak the speaker
  positions and room ambience amounts, for total control. It's
  also *the only* "virtual surround" solution that accurately
  succeeds in making it feel like sounds are *behind* you (which
  is the most difficult direction to achieve correctly).
  
  To use this plugin with headphones, you simply have to create a
  virtual audio cable with 8 channels, and configure it as
  a "speaker pin" in the VAC Control Panel, and then you
  use the Windows "speaker configuration" properties to choose
  that your virtual cable is a "7.1 surround" system, and
  then set that cable as your system's default output device.
  
  After that, you simply need a VST plugin host which is capable
  of routing your virtual cable into the "Waves NX" plugin, and
  then out to your headphones as binaural stereo with amazing
  surround sound feeling.
  
  This will give you surround in all movies and games, and
  what's even more amazing is that Waves NX only uses 0.5-1.6%
  CPU, which is stunning considering what it achieves.
  
* **Best audio plugin host**: "Element Lite" (free) by
  Kushview, from https://kushview.net/element/.
  
  There is simply no other application which can do the same
  thing for free. Element is incredible. It allows you to configure
  an audio input and output, and then lets you hook up virtual
  connectors from every individual channel, through any VST plugins
  you want to use, and then out to your system's audio output.
  
  Be sure that you set its audio mode to "Windows Audio" (*not*
  "exclusive mode"), and set your speakers/headphones as the
  output, and the virtual 7.1 cable as your input.
  
  From there, you will now be able to insert any VST plugins
  you own on your system. Furthermore, Element uses extremely
  little CPU (about 0.2-0.3% for me), and only about 20MB of
  RAM. So there's no doubt that this is the best plugin host
  that you can use for realtime audio purposes.
  
  If you want to insert Waves NX, you should be using the "Waves
  NX 7.1/Stereo" plugin, which converts from surround to stereo.
  
  Have fun!
