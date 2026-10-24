# Wii U streaming performance test plan

This test branch is **experimental**. It changes frame-buffer ownership and
compressed H.264 frame buffering, but has not been verified on real Wii U
hardware. A successful build is not proof of stable 10–20 Mbps streaming.

## Build

Use devkitPro/devkitPPC with the Wii U packages and portlibs listed in the
upstream README, then run:

```sh
git clone --recurse-submodules https://github.com/Firefoxray/firefox-wibelight.git
cd firefox-wibelight
git checkout perf/wiiu-stream-pipeline
git submodule update --init --recursive
make
```

The deployable app is in `dist/wiiu/apps/wibelight/`. Copy the `wiiu`
directory to the SD card root. Save the existing app directory (including
`wibelight.json` and pairing keys) before replacing any files.

## Controlled bitrate sweep

First test a baseline at **854×480, H.264, 60 FPS, 3000 kbps** with a running
high-motion game (not an idle desktop). Keep audio stereo, color range fixed,
and all unrelated settings constant. Then run the in-app benchmark at each
step, changing **only the bitrate**:

| Setting | Trial bitrate (kbps) |
| --- | ---: |
| Baseline | 3000 |
| Trial 1 | 5000 |
| Trial 2 | 8000 |
| Goal 1 | 10000 |
| Trial 3 | 15000 |
| Goal 2 | 20000 |

Repeat the sweep at **1280×720, 30 FPS** first, then **1280×720, 60 FPS** if
the 30 FPS runs pass. Treat 1920×1080/60 as an optional stretch target,
not a prerequisite for 10 Mbps.

For the wired USB-to-Ethernet adapter, compare packet sizes **1024** (current
default) and **1392** (the original Wii U Moonlight configuration) *after*
choosing a working resolution/bitrate. Avoid changing both at once. Ensure
the host is also wired and select a local IPv4 host to avoid VPN routing.

## Record these results

| Resolution/FPS | kbps | Packet size | RTT / jitter | Decode errors | Client queue drops | Actual smoothness / disconnect |
| --- | ---: | ---: | --- | ---: | ---: | --- |
| 854×480/60 | 3000 | 1024 | | | | |
| 854×480/60 | 5000 | 1024 | | | | |
| 854×480/60 | 8000 | 1024 | | | | |
| 854×480/60 | 10000 | 1024 | | | | |

The in-app **Client Queue Drops** count includes frames discarded because
rendering fell behind; this is different from `Decode errors`. Decode
errors, queue drops, and unstable RTT point to different failure areas:

- **Decode errors rise:** investigate H.264 bitstream size, SPS/profile,
  decoder limits, and hardware decoding return codes.
- **Client queue drops rise while RTT stays good:** Wii U decode/render
  throughput may be the bottleneck; try 30 FPS or a lower resolution.
- **RTT/connection status degrades:** investigate the USB Ethernet adapter,
  switches, packet size, or host network path.
- **Stutter with all counters low:** profile latency and host capture/encode
  performance; do not assume bitrate is the only factor.

Do not declare a configuration stable from a still desktop or from the
requested bitrate alone: Sunshine may send lower actual bitrate during low
motion. Test a representative game for several minutes.

## Behavior changed by this branch

- H.264 compressed frame input buffer starts at 256 KiB and grows up to
  2 MiB for large frames. Oversized/invalid units request an IDR rather
  than being silently reported as successfully decoded.
- Decoder uses four output textures and never selects one queued for
  display or still in the renderer. Old queued frames are evicted first.
- UI queue depth choices are 1, 2, and 3; old saved values 4 and 8 are
  clamped to 3 on profile load. The default is 2.
- Benchmark displays client-side queue drops separately from decode errors.

## Known limits

Wii U video decoding, display scheduling, audio, and USB network drivers are
hardware/platform constraints. This branch does **not** add HEVC, AV1, HDR,
adaptive bitrate, or a guaranteed 20 Mbps capability. Test regressions in
pairing, input, TV and GamePad output, exit/reconnect, and benchmarks.
