# Building Volume Linker from Source


## Visual Studio 2019 or Newer

Simply get the free Visual Studio 2019 Community Edition (or newer), and then
open the `VolumeLinker.sln` file that's already included in the root of the
project hierarchy. It is already pre-configured for perfect builds.

All dependencies will be automatically downloaded, since the Visual Studio file
defines the necessary dependencies and grabs them via NuGet effortlessly.

Then simply compile the project in Release mode. Be sure to build both 32-bit
and 64-bit versions, and then simply pick up the `VolumeLinker32.exe` and
`VolumeLinker64.exe` files from the output directories.


## CMake

First you must install several pre-requisites. The easiest way to do that is via
the [Chocolatey](https://chocolatey.org/) package manager. All install commands
are included in parenthesis below. In the future, you will possibly want to
upgrade the "2019" commands below to whatever package is the latest release when
you're reading this. However, it should always be possible to install older
versions of the compilers, so feel free to follow these instructions verbatim.

1. CMake 3.16 or higher (`choco install cmake --installargs 'ADD_CMAKE_TO_PATH=System'`).

2. Visual Studio 2019 Build Tools (`choco install visualstudio2019buildtools`).

3. Latest Visual C++ Compiler and Windows 10 SDK (`choco install visualstudio2019-workload-vctools`).

When you have all required tools above, simply use a command prompt and navigate
to the `VolumeLinker` root folder of the project, and then type the following
commands to compile:

```
cd VolumeLinker

mkdir build32
cd build32
cmake -G "Visual Studio 16 2019" -A Win32 ..
cmake --build . --config Release --target VolumeLinker -j 14
mv ..\build\release\Release\VolumeLinker.exe .\Release\VolumeLinker32.exe

cd ..

mkdir build64
cd build64
cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build . --config Release --target VolumeLinker -j 14
mv ..\build\release\Release\VolumeLinker.exe .\x64\Release\VolumeLinker64.exe
```

The compiled binaries now exist in two locations:

1. `VolumeLinker\build32\Release\VolumeLinker32.exe` for the 32-bit binary.
2. `VolumeLinker\build64\x64\Release\VolumeLinker64.exe` for the 64-bit binary.


## Visual Studio Code (VSCode)

This is my preferred build environment, since it's a very powerful IDE with
support for all kinds of languages, extensions and great configurability!

To build with VSCode, follow these instructions:

1. First install all pre-requisites listed in the `CMake` section above.

2. Inside VSCode, install the following extensions: `C/C++` (by Microsoft),
`CMake Tools` (by Microsoft).

3. Open the `VolumeLinker\VolumeLinker` folder in VSCode. It should automatically
detect the per-project "CMake Configure On Open" setting and ask you to run the
configuration process. Say yes. If it doesn't ask you, press F1 (or Ctrl-Shift-P)
to open the command palette and type `CMake: Configure` and run that command.

4. Next, look at the bottom (status bar) of the VSCode window. Ensure that it's
set to the `CMake: Release` configuration, and that your build kit is set to
`Visual Studio Build Tools 2019 Release - amd64`. Now just click on the `Build`
button in the statusbar (currently defaults to `F7` on the keyboard).

5. After the build is complete, the 64-bit binary will exist in the following
folder: `VolumeLinker\VolumeLinker\build\release\VolumeLinker.exe`. Take it out
of there and manually rename it to `VolumeLinker64.exe`.

6. Repeat step 4 and 5, but use the `Visual Studio Build Tools 2019 Release - x86`
build kit instead, and rename that compiled file to `VolumeLinker32.exe`.

7. Congratulations, you've now compiled the 32-bit and 64-bit versions!

