# aptX Adaptive bridge — code review, bugs and risks

Date: 2026-09-10. Reviewer: the agent that wrote the bridge. Requested by the
user after their own comparison against the Qualcomm implementation
(`~/Documents/aptx-adaptive-vs-qualcomm/`).

Scope reviewed:

- PipeWire codec plugin: `spa/plugins/bluez5/a2dp-codec-aptx-adaptive.c` and
  `a2dp-codec-caps.h` in the `baizhu945/pipewire` fork.
- QEMU helper: `research/aptx-adaptive-qemu/aptx-lossless-helper.c` and
  `compat.c` in this repository.
- OTA parser: `src/aptx-adaptive-stream.c`, `include/aptxadaptive.h`.
- NixOS module: `/etc/nixos/pipewire-aptx-adaptive-module.nix`.
- Runtime configuration: user systemd drop-ins and the runtime bundle in
  `~/Documents/aptx-adaptive-runtime/`.

Every item below was reproduced or read in the deployed revision.

## 1. Bugs found and fixed

### B1 (high) — the helper write path had no deadline

- Where: plugin, `write_full()`.
- Bug: writes to the helper were unbounded. If the helper stops reading its
  stdin (stuck emulator, crashed QEMU, or a helper blocked on its own full
  stdout pipe), the 64 KiB pipe fills and the PipeWire data thread blocks in
  `write()` forever. `read_full()` was already bounded; only writes were not.
- Fix: poll with `APTX_ADAPTIVE_HELPER_WRITE_TIMEOUT_MS` (1 s) and report
  `-ETIMEDOUT`, mirroring the read path.

### B2 (medium) — `APTX_ADAPTIVE_STRIP_OTA` could not be switched off

- Where: plugin, `codec_encode()`.
- Bug: the switch was tested with `getenv(...) != NULL`, so `=0` **enabled**
  stripping. Stripping the 8-byte wrapper is the known-broken wire format.
- Fix: new `env_flag()` helper: unset takes the default, empty means true and
  `0/no/false/off` mean false.

### B3 (medium) — the feature override re-added the R2.2 capability bit

- Where: plugin, `codec_select_config()`.
- Bug: `APTX_ADAPTIVE_FEATURES` rewrote the whole 4-byte feature word *after*
  the deliberate clearing of the R2.2/Lossless bit, so the value shipped in
  the runtime config (`0x0f000092`) restored exactly the bit the code removes.
  With the bit set the wrapper waits for sideband feedback that this bridge
  never sends, and the stream stalls. It only survived because the helper has
  its own backstop.
- Fix: the override is masked again after parsing and a warning is logged; the
  helper backstop stays as defence in depth.

### B4 (medium) — the ABR level mapping was inverted

- Where: plugin, `abr_level_for_unsent()`.
- Bug: `media-sink` passes `get_transport_unsent_size()` (bytes still queued,
  i.e. backlog), but the code treated it as free space, so a congested link
  requested the *highest* quality level.
- Fix: mapping reversed (large backlog maps to a low level), the 0-based to
  1-based conversion documented, and the wrong comment corrected.

### B5 (medium) — the R3 entry asked the R2 wrapper for static properties

- Where: helper, `query_static_properties()`.
- Bug: in R3 mode the memory requirement came from the R2 CAPI wrapper, so
  `aptx_adaptive3_enc_init()` failed with `EIO`. This is why the direct R3
  path never initialised.
- Fix: R3 mode calls `aptx_adaptive3_enc_get_static_properties()`; verified
  that `mode=r3` configuration now returns status 0.

### B6 (medium) — an experimental build forced the OTA version byte

- Where: helper, R3 packet wrapper.
- Bug: an experimental revision always wrote `0xad` into the version byte,
  even when the encoder's own state machine had produced something else. That
  can put an R3 header on a payload the encoder does not consider R3.
- Fix: reverted to substituting `0xad` only when the module left the byte at
  zero. Pinning a header is now an explicit, validated diagnostic (B7).

### B7 (medium) — the version override read a fixed, world-writable path

- Where: helper, OTA version override.
- Bug: the override read `/tmp/aptx_ota_version`, a fixed path under a shared
  directory: any local user could change the version byte of the audio stream,
  and the file outlived the experiment.
- Fix: the path comes from `APTX_OTA_VERSION_FILE` (never a fixed location)
  and only `0xad`, `0xae` and `0xaf` are accepted; anything else is ignored
  with a message.

### B8 (medium) — the helper inherited signals and descriptors

- Where: plugin, `spawn_helper()`.
- Bug: the child kept the parent's blocked signal mask (PipeWire blocks SIGINT
  and SIGTERM), so `reap_helper()`'s graceful `SIGTERM` cannot be delivered and
  every reap degrades to `SIGKILL`. It also inherited every open descriptor,
  so a crashed helper could keep PipeWire sockets alive.
- Fix: the child clears the signal mask, resets `SIGPIPE` to default and
  closes descriptors 3 and above before `exec`.

### B9 (low) — an oversized helper reply was dropped silently

- Where: plugin, `codec_encode()`.
- Bug: a reply larger than the packet buffer was discarded with a success
  return, hiding a protocol error.
- Fix: logged as a warning before the drop.

### B10 (low) — the transport MTU was never sanity-checked

- Where: plugin, `codec_init()` and `codec_encode()`.
- Bug: an MTU below what an Adaptive packet needs makes every packet fail with
  `-EMSGSIZE`, which looks exactly like "the encoder produces nothing".
- Fix: init warns when the MTU is below `APTX_ADAPTIVE_MIN_MTU` (700) and each
  oversized packet is logged before it is dropped.

### B11 (low) — control payloads were almost unbounded

- Where: plugin, `helper_control()`.
- Bug: payload length was only bounded by `UINT32_MAX`.
- Fix: bounded by `APTX_ADAPTIVE_MAX_HELPER_REQUEST` (1 MiB).

### B12 (medium) — the deployed helper was not reproducible

- Where: runtime bundle.
- Bug: the deployed binary was `aptx-lossless-helper-exp`, built from
  `/tmp/helper_ttpaudio.c`, a file that no longer existed. The runtime
  behaviour could not be reproduced from the repository.
- Fix: the environment-gated switches were folded into the tracked
  `aptx-lossless-helper.c` (inert by default), the helper was rebuilt from that
  source, installed as the canonical `helper/aptx-lossless-helper`, and the
  `-exp` binary plus its drop-in override were removed.

### B13 (medium) — the NixOS module forced broken diagnostics

- Where: `/etc/nixos/pipewire-aptx-adaptive-module.nix`.
- Bug: `adaptiveEnv` shipped `APTX_ADAPTIVE_STRIP_OTA=1` (strips the OTA
  header), `APTX_ADAPTIVE_CAPTURE=/tmp/aptx-adaptive-reference.bin` (dumps the
  stream to a shared directory) and `APTX_ADAPTIVE_FORCE_RATE=44100` (resamples
  every 48 kHz source) for every session, including plain aptX HD use.
- Fix: removed from the module; the comment now states that one-shot
  diagnostics belong in user drop-ins.

### B14 (medium) — the encoder environment leaked system-wide

- Where: `/etc/nixos/pipewire-aptx-adaptive-module.nix`.
- Bug: the same variables were exported through `environment.variables`, so
  every process on the machine inherited the encoder configuration. The stale
  copy also survived in the user systemd manager, which is why
  `APTX_ADAPTIVE_FORCE_RATE` kept reappearing after the module was cleaned.
- Fix: only the two audio services receive the environment, and the stale
  values were cleared with `systemctl --user unset-environment`.

### B15 (low) — the reported codec delay was wrong

- Where: plugin, `codec_get_delay()`.
- Bug: it reported 0 samples (7 while downsampling), so the node latency
  ignored the encoder's block delay. The unit is samples, converted by
  `media-sink` with the graph rate.
- Fix: one codec block plus the half-band filter, matching the "one frame"
  convention LDAC uses.

### B16 (low) — a comment claimed the codec order decides priority

- Where: `/etc/nixos/pipewire-aptx-adaptive-module.nix`.
- Bug: the `bluez5.codecs` array is only a whitelist; real priority comes from
  `codec_order()`, where aptX Adaptive ranks above aptX HD.
- Fix: comment corrected, with instructions to delete `aptx_adaptive` from the
  whitelist to keep the currently silent codec opt-in.

## 2. Residual risks

These are accepted and documented; they cannot be fixed in host software.

- **W^X is disabled** for `pipewire.service` and `wireplumber.service` while
  the module is enabled. QEMU TCG must JIT, so the codec cannot run under
  `MemoryDenyWriteExecute=yes`, and the hardening flag is static per unit, so
  it is off even during aptX HD playback. Setting
  `services.pipewire.aptxAdaptive.enable = false` restores it.
- **Proprietary extracted blob.** `aptx_adaptive_enc_module.so.1` is the only
  aptX Adaptive encoder in existence; it never enters the Nix store.
  `README.md` and `STATUS.md` already state that this must not be used as a
  general audio backend without a licence and a real-time review.
- **ABR is non-functional with this build.** The R2.2 CAPI build ignores IMCL
  quality-level feedback outside a full AudioReach container, and the AX210
  exposes no RF/BER feedback, so the stream is fixed-rate (about 210 kbps at
  25 ms). The plugin logs this at init.
- **TTP is host-synthesized.** The wrapper only advances TTP on sink-side
  clock feedback, which this bridge never receives. The default is now the
  audio clock (`samples/rate*15000 + offset`), which is at least
  self-consistent with the RTP timestamps we generate;
  `APTX_TTP_MODE=wall` reproduces the older wall-clock value for A/B tests.
- **R3/Lossless is incomplete.** The R3 kernel's cursors only move forward, so
  the helper refuses with `-EOVERFLOW` instead of crashing, and a forced
  Lossless request is downgraded unless
  `APTX_ADAPTIVE_ALLOW_UNSTABLE_LOSSLESS=1` is set.
- **The sink stays silent with a byte-identical stream.** Unresolved; the only
  remaining difference is the controller and air interface. This is what the
  Ubertooth capture is for.

## 3. Verification performed

1. The helper was rebuilt from the tracked source with the project's own
   `compat.c`, and installed as the canonical runtime binary. Its hash matches
   a fresh build byte for byte.
2. Plain R2 regression at 44.1 kHz: 664-byte packets, OTA
   `... 64 01 00 00 00 ae`, frame header `8300c0a1`, one packet per 25 ms
   block, TTP advancing 375 units per packet from an audio-clock base of 3900
   (the 260 ms the sink reports).
3. Helper switches: `APTX_TTP_MODE=wall` (old behaviour),
   `APTX_OTA_VERSION=0xaf` (applied) and `=0x99` (rejected),
   `APTX_OTA_VERSION_FILE=<path>` (applied), `APTX_FORCE_BITS=16` (switches to
   the 768-byte R2.2 shape), `mode=r3` (configuration succeeds).
4. Plugin: compiled with `-fsyntax-only` before and after the changes; the
   diagnostics are identical, so the edits add no errors or warnings.
5. Deployment: the plugin is loaded from the new store path (checked through
   `/proc/<wireplumber>/maps`), the runtime environment no longer contains
   `STRIP_OTA`, `CAPTURE` or `FORCE_RATE`, and one drop-in remains
   (`SOURCE_TYPE=0x00`, `CHANNEL_MODE=stereo`).
6. End to end: 487 media packets, 656-byte frames, 25 ms cadence, OTA retained
   (`... 64 01 00 00 00 ae`), frame header `8300d0a1` (48 kHz stereo, no longer
   forced to 44.1 kHz) and 217 kbps, matching the format captured from the
   phone.
