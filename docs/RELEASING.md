# Building and publishing a VR release

Publish only when the maintainer explicitly authorizes that release. Do not
include a game installation, personal settings, test imports, SDK installations,
signing credentials or save files. Flat-platform CI is manual and never publishes
releases automatically.

## Source and dependencies

Start from the intended release commit. Follow [the build guide](VR.md) and run
`tools/prepare-vr-deps.ps1`. Dependency revisions are pinned in that script and
CMake. The complete release source bundle puts these same trees under
`build/deps/SDL`, `build/deps/OpenXR-SDK` and `build/deps/glslang`; it is a build
input, not a playable package. Git metadata is not included in that bundle.

For an extracted source bundle, the dependency trees already exist: do not run
the Git-fetch helper. Build host glslang from `build/deps/glslang`, then configure
the Windows VR preset with `FETCHCONTENT_SOURCE_DIR_SDL3` and
`FETCHCONTENT_SOURCE_DIR_OPENXR` set to those absolute source directories. The
Quest Gradle project already uses these locations. CMake applies the included
SDL patch idempotently. The NDK, JDK, Android SDK, Visual Studio tools and Python
are host tools installed separately as described in the build guide.

## Quest APK

1. Set the intended `versionName` and an increasing `versionCode` in
   `quest/app/build.gradle`.
2. Build and check the release:

   ```powershell
   ./quest/gradlew.bat -p quest :app:assembleRelease :app:testDebugUnitTest :app:lintRelease
   ```

3. Sign `quest/app/build/outputs/apk/release/app-release-unsigned.apk` with the
   project's **private release key**, using Android SDK `apksigner`. Run
   `zipalign -c -P 16 4` before signing and `apksigner verify --verbose --print-certs`
   afterwards. See [Android's signing documentation](https://developer.android.com/tools/apksigner).
4. Use the same key for subsequent updates. Store the keystore and its password
   outside the checkout and back them up privately. Never use the Android debug
   key for a public release. Debug installations cannot receive a release-key
   update without migration; document this in the player instructions.

Example, with paths and secrets supplied in your shell environment:

```powershell
apksigner sign --ks $env:OPENCMR2_KEYSTORE --ks-key-alias opencmr2-vr `
  --ks-pass env:OPENCMR2_STORE_PASSWORD --key-pass env:OPENCMR2_KEY_PASSWORD `
  --out dist/OpenCMR2-VR-Quest-arm64-v0.1.0.apk `
  quest/app/build/outputs/apk/release/app-release-unsigned.apk
```

The distributed APK must be non-debuggable, signed and ARM64-only. Include the
runtime notices from `release-assets/licenses`. Test an actual signed release on
a clean device when available; compilation and signature checks do not establish
headset playability. Keep [validation notes](VR-VALIDATION.md) precise about this.

## Windows PCVR

Build **Release**, run CTest and package the executable, SDL3 and the OpenXR loader.
`tools/package_release.py` recursively resolves x64 DLL imports and fails if a
non-system dependency is missing. Install `pefile==2024.8.26` for the packaging
script. Add any required redistributable runtime directory with `--dll-dir`.

Include `tools/Start-PCVR.cmd`, `tools/Start-PCVR.ps1`, `README.md` and
`THIRD_PARTY_NOTICES.md` with repeated `--include` arguments, and the texts in
`release-assets/licenses` with repeated `--license` arguments. Use a fresh
output path under ignored `dist/`. Never copy an entire build or user directory.
Extract the resulting ZIP and run the diagnostic from that extracted copy;
this checks the actual shipped DLLs.

## Source and publication

`tools/package_vr_source.py --output dist/OpenCMR2-VR-Source-v0.1.0.tar.gz`
packages tracked files at HEAD plus the pinned dependency sources, using Git
archives. Commit the final release sources first. The SDL patch is part of the
project and is reapplied at configure time. This avoids putting build products,
local configurations, signing material or original game data into source bundles.

Generate `SHA256SUMS.txt` for the APK, Windows ZIP and complete source bundle.
Inspect every archive's member list, APK identity/signature and native libraries,
and run `git diff --check` before publishing. The build and release instructions
must match the commit used for the binaries. Publish experimental builds as
GitHub **pre-releases**, with known limits and upstream attribution in the notes.

Preserve upstream commit history. The GitHub source archives provide the project
source; the additional source bundle includes pinned dependency sources too.
Do not publish local test logs containing personal paths or original game data.
