# OpenCMR2 VR

Play **Colin McRae Rally 2.0** in VR on **Meta Quest 3**, either running natively
on the headset or through **Windows PCVR / OpenXR**.

This is an experimental VR fork of **[pablocpas/OpenCMR2](https://github.com/pablocpas/OpenCMR2)**,
which is built on **[pablocpas/CMR2Decomp](https://github.com/pablocpas/CMR2Decomp)**.
The original authors made this port possible. Please visit and support their work.

**You must provide your own installed PC copy of Colin McRae Rally 2.0.**
No original game data, cars, tracks, music or videos are included in the downloads.

**[Download the release](https://github.com/DR-89/opencmr2-vr/releases/tag/v0.1.0-alpha.2)**
· [All releases](https://github.com/DR-89/opencmr2-vr/releases)
· [Report a VR issue](https://github.com/DR-89/opencmr2-vr/issues)

## Choose your version

| Version | Where the game runs | Download |
| --- | --- | --- |
| **Quest standalone** | On Quest 3; no gaming PC or streaming connection needed while playing | `OpenCMR2-VR-Quest-arm64-v0.1.0-alpha.2.apk` |
| **Windows PCVR** | On a Windows x64 gaming PC connected to a VR headset | `OpenCMR2-VR-PCVR-Windows-x64-v0.1.0-alpha.2.zip` |

Download these files from **Assets** on the release page. The automatically
generated **Source code** downloads are for developers, not for installing the game.
This release is an **alpha**: a playable race and the import flow have been tested,
but performance and compatibility still need work. See [Known limitations](#known-limitations).

## Install on Quest 3

### 1. What you need

- A Meta Quest 3, its Touch controllers or a compatible USB gamepad.
- A computer and a USB data cable for the initial setup.
- Your own **full PC installation of Colin McRae Rally 2.0**. Console editions,
  demos, an ISO file or an uninstalled CD are not sufficient.
- The Quest APK from this repository's release page.
- Enough free headset storage for the ZIP, an import working copy and the
  extracted game. **About 3 GB free is a useful starting point** for the tested
  approximately 640 MB installation; larger installations need more.

The tested PC installation works without changing its original files. A complete
list of compatible regional editions and patches has not yet been established.

### 2. Install the APK

The USB installation route uses **developer mode** and an APK installer such as
the [SideQuest desktop installer](https://sidequestvr.com/setup-howto).

1. Follow [Meta's device setup guide](https://developers.meta.com/vr/documentation/native/android/mobile-device-setup/)
   to set up your developer account and enable **Developer Mode** for your headset
   in the Meta Horizon phone app.
2. Connect the Quest to your computer with a USB data cable. Put on the headset
   and allow **USB debugging** for your computer.
3. Open the SideQuest desktop installer and make sure it recognizes your headset.
4. Use its **Install APK** function and select
   `OpenCMR2-VR-Quest-arm64-v0.1.0-alpha.2.apk`.
5. Wait for installation to finish. On the Quest, open your app library, find
   **Unknown Sources** and launch **OpenCMR2 VR**. Library labels can vary with
   the headset's system version and language.

If you already use Android platform tools, the equivalent installation command is:

```powershell
adb install -r OpenCMR2-VR-Quest-arm64-v0.1.0-alpha.2.apk
```

Installing the APK installs the VR engine. Import your own game data next.

### 3. Make a ZIP of your PC game installation

1. Find the original game's installation folder on your PC, for example:
   `C:\Program Files (x86)\Codemasters\Colin McRae Rally 2`.
2. Check that it contains folders such as **Game**, **FrontEnd** and **Sounds**.
3. Create a normal, **unencrypted ZIP of the entire installed game folder**.
   In Windows Explorer, right-click the folder and choose the ZIP compression
   option. You can call the result `CMR2-Game-Data.zip`.

A containing folder inside the ZIP is fine. For example, this layout works:

```text
CMR2-Game-Data.zip
└── Colin McRae Rally 2/
    ├── Game/
    ├── FrontEnd/
    ├── Sounds/
    └── ...the rest of your installed game files
```

You can also put the game folders directly at the ZIP root. Use a ZIP, not a
RAR, 7z or ISO. Do not share the ZIP or upload it to this repository.

### 4. Copy the ZIP to your Quest

1. Connect the Quest by USB and allow **file access / file transfer** in the headset.
   This is a separate prompt from USB debugging.
2. In Windows Explorer, open **Quest → Internal shared storage → Download**.
3. Copy `CMR2-Game-Data.zip` there and wait until the transfer finishes.

You do **not** need to copy anything into `Android/data`. The app handles the
installation into its own storage. If you prefer ADB:

```powershell
adb push CMR2-Game-Data.zip /sdcard/Download/CMR2-Game-Data.zip
```

### 5. Import and play

1. Open **OpenCMR2 VR** on the Quest.
2. Choose **Select game data ZIP**.
3. In the system file picker, open **Downloads** and select your ZIP.
4. Wait for **Import complete**, then choose **Start game**.
5. Start a single-player race. Keep the original game's **default keyboard
   bindings**; the VR controls map to those actions. Select manual transmission
   in the game if you want to shift with A and B.

The setup UI is in English; the system file picker follows the headset language.
After a successful import, normal launches go straight into the game. The USB
cable and PC are no longer needed for standalone play. You may delete the ZIP
from **Download** once you have confirmed that the game starts.

## Controls

| Action | Quest Touch | USB gamepad, Xbox-style labels |
| --- | --- | --- |
| Steer / navigate menus | Left stick | Left stick / D-pad |
| Accelerate | Right trigger | RT |
| Brake | Left trigger | LT |
| Shift up / confirm menu selection | **A** | **A** |
| Shift down | **B** | **B** |
| Pause / go back | **Press the right stick** | **Press the right stick (R3)** |
| Handbrake | Right grip | X |
| Change camera | Y | Y |
| Recenter seated view | Press the left stick | Press the left stick (L3) |

The right stick's direction does not change gears. B does not open the pause menu.
Connect a USB gamepad to the Quest through an appropriate USB-C adapter, or to
the PC for PCVR. SDL-compatible gamepads are supported; physical USB gamepads on
Quest still need hardware validation. Wheels, pedals and force feedback are not
validated in this release.

## Play through Windows PCVR

1. Install your own PC copy of Colin McRae Rally 2.0.
2. Download the **PCVR Windows x64 ZIP** and extract it completely into a writable
   folder, such as `C:\Games\OpenCMR2-VR`. Do not run it inside the ZIP.
3. Connect your headset to the PC and start your OpenXR runtime. **Virtual Desktop
   with Quest 3 is the tested configuration.** Other OpenXR connections have not
   yet been validated for this release.
4. Double-click **Start-PCVR.cmd** in the extracted folder.

OpenCMR2 VR looks for the installed game through the Windows registry and common
game folders. A single valid match is remembered automatically. If no match or
multiple installations are found, choose the desired game folder once. Files
stay in their original location. **PCVR does not require a ZIP import or game data
on the Quest.**

Saves, settings and logs are in the package's `user` folder. Keep that folder when
updating. For a different data location, use `Start-PCVR.ps1 -Data 'D:\Games\CMR2'`.
Running `opencmr2.exe --no-vr` starts the original flat renderer.

## Updates, saved games and re-importing

- Install a newer APK from this project over the existing release to keep data
  and saves. **Uninstalling removes the app's stored data**; keep your source
  installation and back up saves before uninstalling.
- Earlier private debug builds use a different signing key and cannot be
  updated in place with the public release APK. Back up saves before migrating;
  a clean installation and another game-data import are required.
- Open the second library entry, **OpenCMR2 VR · Game data**, to import different
  data or run **VR test without game data**. Close the running game before importing.
- Replacement imports preserve saves and only replace existing game data after
  successful validation. Quest saves/settings are under
  `Android/data/org.opencmr2.vr/files/User`; access can depend on the headset OS.
- When updating PCVR, extract the new package and retain your previous `user`
  folder. Your original PC installation remains separate.

## Troubleshooting

| Problem | What to try |
| --- | --- |
| Quest is not detected by the APK installer | Use a data-capable USB cable, enable developer mode and accept USB debugging inside the headset. Follow Meta's linked setup guide for Windows drivers if needed. |
| ZIP is missing in the file picker | Finish copying it first, then look in **Downloads**. Use `.zip`, not another archive format. |
| Import reports missing files | ZIP the complete installed PC game. Required nonempty archives include `Game/Bigfiles/common.bfl`, `FrontEnd/common.bfl` and `Sounds/surface.bfl`. |
| Import reports insufficient space | Free storage for the ZIP, working copy and extracted data. A replacement also temporarily keeps the old data. The dialog reports the required space. |
| Import was interrupted | Reopen the setup entry. The importer recovers an interrupted replacement; select the ZIP again if needed. |
| APK update fails with a signature mismatch | The installed app was signed with another key, such as a private debug build. See the migration note above before uninstalling. |
| PCVR cannot start OpenXR | Connect the headset and activate the runtime for your VR connection before launching. Virtual Desktop is the tested route. |
| Steering or shifting uses the wrong actions | Restore the game's default keyboard controls and choose manual transmission for manual shifting. Press L3 to recenter. |

The importer accepts ZIPs up to 4 GiB, extracted data up to 8 GiB and up to
50,000 archive entries. It checks required files, paths, CRCs and free space.

## Known limitations

- **Quest performance:** this release requests **120 Hz** and reaches roughly
  **120 FPS** in the tested stage sections at the same eye resolution as alpha.1.
  The measured steady driving sample averages **119.97 FPS**; occasional missed
  frames remain, and this does not guarantee 120 FPS on every stage or during
  long sessions. See the [performance notes](docs/QUEST-PERFORMANCE.md) and
  [measured validation results](docs/VR-VALIDATION.md).
- Quest 3 standalone gameplay and Windows PCVR gameplay have been confirmed on
  hardware. The initial user tests used development builds; release packaging
  checks are documented separately in [validation notes](docs/VR-VALIDATION.md).
  The latest renderer was retested on Quest; a new PCVR headset test is pending.
- Only the Touch interaction profile has motion-controller bindings. Other
  headsets and Quest models are untested. Split-screen is not adapted for VR.
- Extended races, finish/save/load, suspend/resume, controller reconnects and
  extreme head movement need more testing. Multiplayer is not validated in VR.

When reporting an issue, include the version, standalone or PCVR mode, headset,
runtime and reproduction steps. Share logs only after checking them for personal
paths. **Do not attach original game files.**

## Source, build instructions and credits

- [VR build guide](docs/VR.md) · [Release build and signing guide](docs/RELEASING.md)
- [Original OpenCMR2 repository](https://github.com/pablocpas/OpenCMR2)
- [CMR2Decomp](https://github.com/pablocpas/CMR2Decomp)
- [Preserved upstream README](docs/UPSTREAM-README.md)
- [Third-party notices](THIRD_PARTY_NOTICES.md) · [GPL-3.0 license](LICENSE)

This fork starts from OpenCMR2 commit `a51f47f216af16fec8959edcf83694ea22cbc0fe`.
The release includes a source bundle with the pinned SDL/OpenXR dependencies and
build instructions. Original game files are required at runtime and are never
part of that bundle. This is a community project, unaffiliated with Codemasters,
EA or Meta.
