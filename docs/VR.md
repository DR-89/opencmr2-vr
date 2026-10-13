# OpenCMR2 VR

Community VR fork of [OpenCMR2](https://github.com/pablocpas/OpenCMR2), based on
commit `a51f47f216af16fec8959edcf83694ea22cbc0fe`. For player installation
instructions, start with the [README](../README.md). Maintainer publication and
signing steps are in [RELEASING.md](RELEASING.md).

## Targets and implementation

- Quest 3: native Android ARM64 APK, Vulkan, OpenXR, SDLActivity for lifecycle,
  audio and USB HID gamepads. Package: `org.opencmr2.vr`.
- PCVR: native Windows x64 executable, Vulkan, the active OpenXR runtime.
- Two eye views with runtime FOV and IPD, positional and rotational head tracking,
  seated position/heading recenter. Simulation runs once; rendering replays twice.
- Original 2D menus, videos and frontend 3D are shown on an OpenXR quad. Stage
  geometry uses stereo projection; the HUD uses a virtual panel in the stage.
- Touch controller actions and SDL gamepads feed the original analog driving
  values. The active device supplies all axes together. Focus loss clears VR
  driving input; unplugged gamepads release their values.
- A desktop mirror retains the original game view. Reflection targets are shared
  by the eyes. Texture compression falls back to upstream's CPU decoding on GPUs
  without BC texture support.

This is a development implementation. Compilation and unit tests do not establish
playability. Race camera placement, sky/effect rendering, HUD occlusion, menus,
save entry, disconnect/reconnect and frame timing need tests on real hardware.
The rendering path submits work to SDL's Vulkan queue before releasing images
to OpenXR, which synchronizes GPU completion; queue access remains on one thread.
This follows the [Khronos clarification](https://github.com/KhronosGroup/OpenXR-Docs/issues/177).
Quest skips the unneeded mono stage draw unless a readback requests it;
PCVR retains a desktop mirror. Optimize further after measuring on Quest. Only the
Oculus Touch interaction profile has motion-controller bindings; other PCVR
headsets may use a supported USB gamepad. Split-screen has not been adapted.

## Dependencies

VR uses SDL's new GPU/OpenXR interop, pinned to commit
`5180c654d439ad7df5eb99481fd0e3f027db65de` (development SDL, not the stable 3.4 API).
The Khronos loader/headers are pinned to
`f2448a8797c85814aa892efc1ab8707900fbcc78` (OpenXR 1.1.63).
The application requests OpenXR 1.0 core plus SDL's Vulkan extension.
SDL initializes the Android loader using the Activity and JavaVM.
CMake applies `cmake/patch_sdl_openxr.cmake` to initialize the API-layer output
structure headers missing from the pinned SDL revision. This also works with
the dependency trees in the complete release source bundle.

Run `tools/prepare-vr-deps.ps1` to fetch the fixed revisions under `build/deps`.
No game data is downloaded. Licenses remain in the upstream source trees:
OpenCMR2 GPL-3.0, SDL zlib, OpenXR Apache-2.0, glslang BSD-style notices.

## Windows build

Use a Visual Studio x64 developer shell, Clang with GNU command-line syntax,
CMake 3.24+ and Ninja. Put `glslang` or `glslangValidator` in PATH. A local shader
compiler can be built with:

```powershell
./tools/prepare-vr-deps.ps1
cmake -S build/deps/glslang -B build/host-glslang -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_OPT=OFF
cmake --build build/host-glslang --parallel 8
cmake --preset windows-vr -DGLSLANG_VALIDATOR="$PWD/build/host-glslang/StandAlone/glslang.exe"
cmake --build --preset windows-vr --parallel 8
ctest --preset windows-vr
```

The local execution environment may supply Clang from the Android NDK. In that
case target `x86_64-pc-windows-msvc` explicitly for both C and C++, and select
`-DCMAKE_LINKER_TYPE=MSVC` (CMake 3.29+) to use the installed Microsoft linker.
`clang-cl` is not supported by the decompiled game.

Start the headset's OpenXR runtime, then:

```powershell
./build/windows-vr/src/opencmr2.exe --vr-test --user-dir ./build/vr-test-user
./tools/Start-PCVR.ps1
```

The first command needs no game data. It displays cubes at 2, 4 and 6 metres;
left stick moves the white cube, right trigger raises green, left trigger raises
red. Turn and lean to check stable geometry and eye separation. Right stick click
or Escape exits.
The second starts the actual game and keeps settings and saves in the build's
`user` folder. The launcher also works next to a packaged `opencmr2.exe`; the
local test copy is `build/pcvr/Start-PCVR.ps1`. `--no-vr` on the executable selects
the original flat renderer. An unavailable runtime is reported as an initialization
failure.
For double-click startup, use the adjacent `Start-PCVR.cmd`. Keep both launcher
files together when copying them next to a packaged executable.

### Automatic PC game setup

An installed PC copy is enough for the usual Windows layout; no ZIP import or
copy to the Quest is needed for PCVR. After checking explicit, portable and saved
paths, OpenCMR2 checks the 32-bit and 64-bit Codemasters/uninstall registry entries
under the current user and local machine. It also checks common game names under
Program Files, Codemasters, Games and GOG Games on local fixed drives. The search
is bounded; it does not recursively scan whole disks or probe network/CD drives.

Every candidate must contain the nonempty PC archives `Game/Bigfiles/common.bfl`,
`FrontEnd/common.bfl` and `Sounds/surface.bfl`. Duplicate paths to the same folder
count as one installation. A unique match is saved as `[paths] data` in
`opencmr2.ini` and the game starts directly. If the saved folder later disappears,
the search runs again. Other settings and original game files are preserved.

If there is no match, or several installations exist, an English setup dialog
asks the user to select a folder once. Unusual locations and explicitly chosen
external/network folders remain supported by the picker. A missing/unwritable
settings file produces an error instead of silently losing the selection.
To override the location, run `./tools/Start-PCVR.ps1 -Data 'D:/My Games/CMR2'`
or pass `--data <folder>` directly to the executable.

For an automated GPU smoke test, set `OPENCMR2_VR_TEST_FRAMES=30` and
`OPENCMR2_VR_TEST_CAPTURE` to an output BMP path, then run `--vr-test --no-vr`.
This checks the recorder and Vulkan renderer without requiring a connected
headset; it does not validate stereo or an OpenXR runtime.

## Quest APK

Requirements: JDK 17+, Android SDK platform 35, build tools 35.0.0, NDK
29.0.14206865, CMake 4.4.0 and Ninja. The standalone `quest/` project avoids the
upstream phone touch UI. Set `sdk.dir` and `cmake.dir` in `quest/local.properties`
for your installed SDK and CMake; this file is ignored. Install missing SDK
components through your SDK manager. The project does not accept SDK licenses
on your behalf.

```powershell
./tools/prepare-vr-deps.ps1
# Build host glslang as above, or set GLSLANG_VALIDATOR to its absolute path.
./quest/gradlew.bat -p quest :app:assembleDebug
```

Output: `quest/app/build/outputs/apk/debug/app-debug.apk`. This is a local debug
APK for sideload testing, not a store release. Release signing is not configured.
The native code uses `RelWithDebInfo` optimizations even in the debug APK. The
in-app ZIP importer also works without the debug-only `run-as` transfer tool.

Enable developer mode and USB debugging on the Quest, connect USB and authorize
the computer inside the headset. Verify `adb devices` lists the intended device.
For the asset-free test:

```powershell
adb install -r quest/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n org.opencmr2.vr/.VrActivity --ez vr_test true
adb logcat -s SDL OpenXR-Loader
```

For the game, provide your installed original **PC full version** of Colin McRae
Rally 2.0. The upstream port does not specify a required patch version. The data
root must contain at least `Game/Bigfiles/common.bfl`, `FrontEnd/common.bfl` and
`Sounds/surface.bfl`, along with the rest of the installation. Console versions
are not the expected format.

### Import on the headset

1. Install the APK and create a normal, unencrypted ZIP of the complete installed
   PC game folder. A containing folder inside the ZIP is supported.
2. Connect the Quest by USB, allow file transfer and copy the ZIP to `Download`.
3. Open **OpenCMR2 VR**. On a new installation a 2D setup panel appears. Choose
   **Select game data ZIP**, then select the ZIP in Android's file picker.
4. The app reads the ZIP, checks the required archives and free space, then
   imports with progress and cancellation. It checks each extracted file's size
   and CRC, rejects unsafe/ambiguous paths and replaces existing data only after
   successful validation. Interrupted imports are recovered on the next setup
   launch. Saves and settings are stored separately.
5. Select **Start game**. Subsequent normal launches start the game directly.
   The original ZIP is left in Download; the user can delete it to reclaim space.

Use the second launcher entry **OpenCMR2 VR · Game data** for replacement imports
or **VR test without game data**. Replacement imports require closing the native
game first; an interprocess lock prevents changing files used by a running race.
The importer uses Android's document picker and private app storage; it needs
neither broad storage access nor a PC-side Python/ADB importer after installation.
Imports are limited to a 4 GiB ZIP, 8 GiB extracted data and 50,000 entries, with
64 MiB free space reserved. During import allow space for the private ZIP copy,
new extracted data and any existing installation until replacement completes.
Required-archive checks and ZIP checksums cannot establish compatibility with
every original game edition or modification; gameplay still needs testing.

The German walkthrough is in [QUEST-IMPORT-DE.md](QUEST-IMPORT-DE.md). The setup
uses Horizon's [hybrid panel/immersive activity flow](https://developers.meta.com/vr/documentation/spatial-sdk/hybrid-apps-overview/)
and Android's [document picker](https://developer.android.com/training/data-storage/shared/documents-files).

### Developer transfer alternative

The original debug-only data-transfer script remains available:

```powershell
./tools/install-quest.ps1 -Data 'D:/Games/Colin McRae Rally 2.0'
```

The script requires Python 3 (override with `-Python`), installs the local APK,
and imports data into the debug app's private `files/Data` directory. Supply
`-Serial` if several ADB devices are connected. The importer uses ADB sync and
`run-as`, checks the required archive checksums, and removes its temporary tar
files. This works with recent Quest firmware that rejects direct ADB writes into
`Android/data`. This alternative requires a debuggable APK; normal users can use
the in-app ZIP importer described above.

Saves, configuration and logs remain under
`/sdcard/Android/data/org.opencmr2.vr/files/User`. Reinstall with `adb install -r`
to retain data; uninstalling deletes app data. To leave the test and launch the
game, force-stop the test first, then launch without the `vr_test` extra.

## Controls

| Action | Quest Touch | USB gamepad (SDL standard mapping) |
|---|---|---|
| Steering / menu left-right | Left stick X | Left stick X / D-pad |
| Menu up-down | Left stick Y | Left stick Y / D-pad |
| Throttle | Right trigger | Right trigger |
| Brake / reverse through original gearbox | Left trigger | Left trigger |
| Handbrake | Right grip | X / West |
| Confirm | A | A / South |
| Back / pause | Right stick click | Right stick click |
| Shift up | A | A / South |
| Shift down | B | B / East |
| Camera cycle | Y | Y / North |
| Recenter seated view | Left stick click | Left stick click |

This default bridge uses the original keyboard action bindings for buttons and
overrides the first driver's analog axes. Keep the game's default keyboard
bindings and first-player keyboard selection for this initial version. Original
joystick configuration remains available in flat mode. Device switching is
automatic when an input is used; USB takes priority if both are moved at once.
The original game consumes a button's rising edge for each shift. A also confirms
menu choices; right stick click goes back or opens/closes the pause menu. B only
shifts down, so downshifting does not pause the race. Right stick movement has no
gear binding. Choose manual transmission in the game's driving options to
control gears yourself.

In `opencmr2.ini`:

```ini
[vr]
enabled = 1
resolution_percent = 80
; Quest default: request 120 Hz. PCVR default: 0 (keep runtime preference).
refresh_hz = 120
; Optional CPU elapsed-time breakdown in the log; 0 by default.
frame_stats = 0
world_scale_percent = 100
gamepad_index = 0
```

Resolution is clamped to 40–150% of each runtime-recommended eye size. World scale
is 10–1000%; 100 means one game unit per metre and needs in-car confirmation.
Recenter from the intended seated driving position.

The runtime must advertise a refresh rate before the app requests it. Unsupported
rates fall back to the highest advertised rate below the request; `0` keeps the
runtime preference. The log records both the request and the actual refresh-rate
change. A 120 Hz display setting alone does not establish 120 rendered FPS.
See [Quest performance notes](QUEST-PERFORMANCE.md) for the rendering changes and
on-device measurement procedure. The mono-camera lens flare is disabled in VR.

## Validation required before release

See [VR-VALIDATION.md](VR-VALIDATION.md) for actual local build and headset results.

Run automated tests (including `vr_math` and `vr_input`) first. On the headset,
check the diagnostic scene, then a full stage: menu navigation, stereo at near
and far objects, turning behind, positional movement, each driving control,
recenter, pause/resume, system overlay, headset sleep/wake, USB unplug/reconnect,
audio, finishing a stage and saves. Measure CPU/GPU frame times at the headset's
actual refresh rate. Verify PCVR separately. A successful initial race does not
establish that the remaining gameplay, comfort and lifecycle checks pass.
