# OpenCMR2 VR

- Work locally. Do not push, create remote repositories or pull requests, publish
  releases, upload builds, or deploy anything until the user explicitly authorizes it.
- Targets: native Android ARM64 on Quest 3 and Windows x64 PCVR via OpenXR.
- Support both Quest Touch controllers and connected USB gamepads.
- Never commit original game assets, signing keys, or local SDKs.
- Keep game adaptations small, annotated with `// PORT:`, and preserve flat mode.
- Report compilation, automated tests, and headset gameplay validation separately.
