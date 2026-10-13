# VR validation

Base game revision:
`a51f47f216af16fec8959edcf83694ea22cbc0fe`.

## Verified on 13 October 2026

- Windows x64 Release build completes. GNU-style Clang/MSVC ABI requires the
  Khronos loader's `.def` export list; otherwise the DLL has no entry points.
  The `vr_loader` regression test checks the four loader globals SDL needs.
- All 20 available CTest cases pass, including projection, IPD, recenter, input
  focus loss, virtual USB hotplug and complete-device input arbitration.
- Windows Vulkan diagnostic renders 30 frames and produces a valid backbuffer
  capture. This is a flat GPU test, not a Windows headset test.
- The supplied original PC data passes a headless real-game run: menus, stage
  loading, 500 simulation ticks including 375 driving ticks.
- Native ARM64 debug APK installs on Quest 3. OpenXR reports Oculus runtime and
  two 1344 x 1408 eye images at the default resolution setting.
- The tester confirms depth, head rotation/translation and both trigger/steering
  controls in the headset diagnostic. Runtime telemetry reports 72 FPS there.
- Original data imported into private app storage; the three required archives'
  SHA-256 hashes match the PC copy. Original videos and menus run on Quest.
- The tester confirms the first race starts and the Touch driving controls work.
  A headset capture also shows the stage, car and pause menu in both eyes.
- Native optimization and skipping the unnecessary mono stage draw raised the
  measured race rate from 11–12 to approximately 46–55 FPS. The tester confirms the
  optimized build feels much smoother and rendering and controls still work.
  The subsequent build submits GPU work without a blocking CPU fence before
  OpenXR release; the latest measured scene ran at 46–47 FPS.
- Updated controls: A shifts up and confirms menus, B shifts down, right stick
  click sends back/pause. Right stick movement no longer shifts. Windows and
  Android rebuilds succeed, and all three VR CTest cases pass with checks that
  downshift does not pause and right stick click does not shift. The updated APK
  is installed locally on Quest and reaches the focused OpenXR session state.
  The latest mapping is confirmed in the Windows PCVR race below; its separate
  native Quest race check is still pending.
- Windows PCVR diagnostic runs through VirtualDesktopXR on the RTX 3050 Laptop
  GPU with two 1689 x 1843 eye images. The tester confirms stereo, head movement and
  controller inputs. The diagnostic closes cleanly after 2001 frames.
- The Windows game launches with the original PC data through the same runtime,
  enters a focused OpenXR session and plays the original intro videos. The tester
  confirms a PCVR race with manual transmission: stereo, steering, throttle,
  brake, A to shift up, B to shift down and right stick click for pause all work.
  Local launcher: `build/pcvr/Start-PCVR.ps1`; connect Virtual Desktop first.
- The in-app Quest ZIP importer builds in both debug and unsigned release APKs.
  All 14 Java importer tests pass: root/nested folders, case variants, replacement,
  save preservation, unsafe paths, duplicate/conflicting names, missing/empty
  archives, ambiguous installations, corrupt ZIP/CRC, storage failure,
  cancellation, incomplete/oversized input, unknown input size, crash recovery
  and the running-game lock. Android lint reports zero errors; SDL's optional BLE
  code has two file-scoped MissingPermission exclusions because SDL gates its
  Bluetooth initialization on permission checks. Warnings remain (43).
- At the tester's request the setup menu, buttons, progress and errors are English.
  The Android document picker is a system component and follows system language.
  Existing data initially skipped import. The tester then explicitly requested deleting
  the Quest game data to test a fresh installation; the PC source, Downloads ZIP
  and separate User/save directory were retained.
- The tester confirms the full first-install flow: select the ZIP, reach **Import
  complete**, then **Start game** successfully enters the native game. The actual
  local ZIP contains a wrapping `Colin McRae Rally 2/` folder. Device logs confirm
  906 files / 638,230,202 bytes imported. The three required archive SHA-256s match
  the PC source, the staging/backup directories are cleaned, and the VR process
  starts. This test used the debug APK through the ordinary document picker, not
  the developer transfer script. Signed release installation remains untested.
  Evidence: `build/quest-import-device.log`, `build/quest-import-verification.json`,
  `build/quest-import-english-build.log` and the Gradle unit-test report.

- Windows automatic game discovery builds successfully; the shared platform
  change also compiles in the Quest ARM64 debug APK. All 20 native CTest cases
  pass. The expanded `game_data_setup` case covers unique matches, invalid
  candidates, Unicode paths, duplicate aliases, saved/explicit precedence,
  moved installations, ambiguous matches, manual fallback and save failures.
- A fresh disposable Windows user directory and no data override find the real
  installed PC copy at `C:/Program Files (x86)/Codemasters/Colin McRae Rally 2/`
  and persist its path. The existing headless stage probe then loads the game
  and completes 500 simulation ticks, including 375 driving ticks. Both registry
  discovery and common-directory candidates run against the actual machine;
  the registry contains the original 32-bit `Game_HDPath` value. Evidence:
  `build/pcvr-discovery-live.log`, `build/pcvr-discovery-live-user/opencmr2.log`,
  `build/pcvr-discovery-tests.log`, `build/pcvr-discovery-build.log` and
  `build/quest-discovery-build.log`.
- The PCVR launcher no longer embeds a personal game-data path. Tracked
  `tools/Start-PCVR.ps1` and its double-click `Start-PCVR.cmd` wrapper are copied
  to `build/pcvr/`. PowerShell syntax is validated. No additional headset race
  or Quest APK installation was performed for this Windows setup change; the
  hardware gameplay and ZIP-import confirmations above refer to earlier builds.

- A normal Unknown Sources launch initially produced audio with passthrough and
  no visible game. Meta's runtime logged a mismatch between the package and the
  `org.opencmr2.vr:vr` process. Moving the native VR activity to the default app
  process and the importer to `:setup` removes that mismatch. The tester confirms
  that launching the corrected debug APK through Unknown Sources restores both
  picture and sound. This correction is included in the release source/APK.
- Release preparation: Windows Release compilation and all 20 native tests
  pass. The Quest release build and all 14 importer tests pass; release lint has
  zero errors and 43 warnings. The public APK is ARM64-only, non-debuggable and
  signed with a dedicated private release key. Signature and ZIP alignment are
  checked. The key and all original game files remain outside published assets.
  The signed release itself has not replaced the differently signed private
  debug app on the test headset; the confirmed launcher test used the same code
  in the debug build, preserving the tester's installed data.
- The final Windows ZIP is extracted into a fresh directory and its executable
  renders 30 Vulkan diagnostic frames, exits successfully and produces a visually
  checked image. Recursive PE dependency checks include the dynamically loaded
  OpenXR loader and Microsoft x64 runtimes. This is a flat GPU/package smoke
  test, separate from the earlier PCVR headset gameplay confirmation.

## Remaining validation and known limits


- Race performance is still below the headset's 72 Hz refresh rate. The 72 FPS
  diagnostic result must not be presented as the race frame rate.
- Windows diagnostic tracking/input and the initial PCVR race are confirmed on
  the headset. PCVR frame timing and extended gameplay remain unmeasured. The initial test builds logged API-layer property-enumeration errors, traced
  to uninitialized structure headers in the pinned SDL revision. Release builds
  apply the small correction in `cmake/patch_sdl_openxr.cmake`.
- A physical USB gamepad on Quest has not been tested. The SDL virtual-device
  tests establish the bridge behavior, not every real controller's mapping.
- Finish/save/load, extended driving, camera variants, head tracking at extreme
  angles, sleep/wake and USB reconnect need further on-device checks.

Build and runtime logs are local under `build/`; no original game assets are in
the repository or APK. Installation retains app data; uninstalling removes it.
