# Repository agent notes

This is a Wii U homebrew Moonlight client in C, built using devkitPro wut and
devkitPPC (see Makefile). `third_party/moonlight-common-c` is a Git submodule:
always clone/update with `--recurse-submodules`.

Prioritize correctness over optimistic bitrate claims. The Wii U H.264
decoder and GX2 texture renderer share output surfaces across threads.
Never allow the decoder to overwrite a frame while it is queued or being
rendered. The available texture count bounds the queue depth. Enforce a
strict max on compressed input sizes and validate scatter/gather lengths
before copying. Preserve the original pairing, JSON profiles, sound, controller
support, and TV/GamePad drawing behavior.

Build using the devkitPro toolchain rather than native GCC. Do not assume a
native Linux build is a Wii U validation. Run `make clean && make` after
changing headers, and check `dist/wiiu/apps/wibelight/wibelight.wuhb`.
Do not silently change network packet sizes, Sunshine host settings, or H.264
profile compatibility. Only claim achieved 10/20 Mbps after real Wii U
testing with high-motion gameplay, decode error counts, and queue-drop counts.
See `docs/performance-testing.md` for the controlled test matrix.
