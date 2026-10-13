# Quest performance work

These rendering improvements are included in `v0.1.0-alpha.2`. The earlier
`v0.1.0-alpha.1` release contains the previous renderer.

## Rendering changes

- Request 120 Hz through the advertised OpenXR display-refresh extension on
  Quest. PCVR keeps its runtime preference unless `vr.refresh_hz` is set.
- Request sustained CPU/GPU performance through OpenXR. The runtime remains
  responsible for clock speeds and thermal limits.
- Disable the old mono-camera lens flare in VR. Its visibility test forced an
  extra mono stage render and a blocking GPU readback. Sun lighting remains.
- Draw directly into the acquired OpenXR eye images, removing two full-eye
  texture copies and a queue submission per world frame.
- Test conservative geometry bounds against each tracked eye's frustum. This
  avoids submitting invisible objects while retaining side/rear views when the
  user turns their head. It does not reduce draw distance.
- Reuse geometry upload allocations and unchanged shader uniform data. Skip
  calculations for disabled lights and avoid setting lookups per draw.
- Combine adjacent compatible triangle lists without changing draw order,
  material boundaries, transparency order, or 16-bit index limits.
- Use simpler pixel shaders for matching single-texture materials. Shaders for
  materials without alpha testing omit discard instructions; other materials
  retain the general fixed-function emulation.
- Render frontend panels once instead of replaying them in both unused eyes.

The tested eye resolution remains **1344 x 1408 per eye**, the existing 80%
runtime-recommended setting. No dynamic resolution, frame generation, or
application spacewarp was added.

## Repeatable measurement procedure

1. Connect the Quest over ADB and enable `[vr] frame_stats = 1` in the app's
   `User/opencmr2.ini`. Keep a backup of the file.
2. Start a race and drive through dense sections. Confirm textures, alpha edges,
   fog, stereo, head turns, controls and pause/resume. Let loading and first-use
   pipeline compilation settle before taking the steady-state measurement.
3. Capture `adb logcat -d -s VrApi:I SDL/APP:I '*:S'` to a local log. Capture the
   app PID (`adb shell pidof org.opencmr2.vr`) to distinguish successive builds.
4. Run `python tools/quest_perf_summary.py quest.log --pid PID --warmup 6`.
5. Inspect the runtime's `FPS`, refresh denominator, `Stale`, GPU workload and
   thermal values as well. CPU `wait` includes runtime pacing/swapchain waits;
   it is not simulation cost. CPU numbers are elapsed time, not GPU timing.

The summary reports **three-second FPS windows**, not individual frame-time
percentiles. Driving different paths or looking in different directions changes
workload, so these sessions are not a deterministic A/B benchmark. Menus, videos,
loading transitions and the cube diagnostic must not be reported as race FPS.

Performance evidence and headset confirmation are recorded in
[VR-VALIDATION.md](VR-VALIDATION.md). Long sessions, other stages and PCVR require
their own measurements.

## API references

- [OpenXR refresh-rate requests](https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrRequestDisplayRefreshRateFB.html)
- [OpenXR performance settings](https://registry.khronos.org/OpenXR/specs/1.0/man/html/XR_EXT_performance_settings.html)
- [Meta hybrid activity transitions](https://developers.meta.com/vr/documentation/spatial-sdk/hybrid-apps-overview/)
