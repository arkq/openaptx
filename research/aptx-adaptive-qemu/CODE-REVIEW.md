<!-- markdownlint-disable MD013 -->
<!-- The finding tables carry file:line and evidence columns that cannot
     be wrapped to 80 columns. -->
# aptX Adaptive bridge — code review, bug list and risk register

Date: 2026-09-10 · Reviewer: the agent that wrote the bridge · Requested by the
user after their own comparison against the Qualcomm implementation
(`~/Documents/aptx-adaptive-vs-qualcomm/`).

Scope reviewed:

| Artefact | Path |
| --- | --- |
| PipeWire codec plugin | `~/work/pipewire/spa/plugins/bluez5/a2dp-codec-aptx-adaptive.c`, `a2dp-codec-caps.h` |
| QEMU helper | `~/work/openaptx/research/aptx-adaptive-qemu/aptx-lossless-helper.c`, `compat.c` |
| OTA parser | `~/work/openaptx/src/aptx-adaptive-stream.c`, `include/aptxadaptive.h` |
| NixOS module | `/etc/nixos/pipewire-aptx-adaptive-module.nix` |
| Runtime config | `~/.config/systemd/user/{pipewire,wireplumber}.service.d/*.conf` |
| Runtime bundle | `~/Documents/aptx-adaptive-runtime/helper/` |

Everything below was reproduced or read in the deployed revision; nothing is
based on recollection. Findings marked **fixed** are implemented in this round.

## 1. Bugs found and fixed

| # | Sev | Where | Bug | Fix |
| --- | --- | --- | --- | --- |
| B1 | **high** | plugin `write_full()` | Writes to the helper had **no deadline**. If the helper stops reading stdin (stuck emulator, crashed QEMU, or a helper blocked on its own full stdout pipe), the 64 KiB pipe fills and the PipeWire data thread blocks in `write()` **forever**. `read_full()` was already bounded — only the write path was not. | `write_full()` now polls with `APTX_ADAPTIVE_HELPER_WRITE_TIMEOUT_MS` (1 s) like the read path, and reports `-ETIMEDOUT`. |
| B2 | med | plugin, OTA strip | `APTX_ADAPTIVE_STRIP_OTA` was tested with `getenv(...) != NULL`, so **`=0` enabled stripping** and there was no way to switch it off from the environment. Stripping the 8-byte wrapper is the known-broken wire format. | New `env_flag()` helper: unset = default, empty = true, `0/no/false/off` = false. |
| B3 | med | plugin, `codec_select_config()` | `APTX_ADAPTIVE_FEATURES` rewrote the whole 4-byte feature word **after** the deliberate clearing of the R2.2/Lossless capability bit, so the shipped value `0x0f000092` re-added exactly the bit the code removes (stream stalls when the wrapper waits for sideband feedback). It only worked because the helper has its own backstop. | The override is masked again after parsing, with a warning; the helper backstop stays as defence in depth. |
| B4 | med | plugin, `abr_level_for_unsent()` | The ABR level was **inverted**: `media-sink` passes `get_transport_unsent_size()` (bytes still queued, i.e. backlog), not free space, so a congested link was mapped to the *highest* quality level. | Mapping reversed (large backlog → low level) and the comment corrected; 0-based → 1-based conversion documented. |
| B5 | med | helper, `query_static_properties()` | In R3 mode the helper asked the **R2 CAPI wrapper** for static properties, so the init memory requirement belonged to the wrong kernel and `aptx_adaptive3_enc_init()` failed with `EIO` (this is why the direct R3 path never initialized). | R3 mode now calls `aptx_adaptive3_enc_get_static_properties()`. Verified: `mode=r3` config now returns status 0. |
| B6 | med | helper (R3 wrap) | An experimental build **forced** `packet[7] = 0xad` regardless of what the module's state machine had produced, i.e. it could emit an R3 header on a payload the encoder did not label as R3. | Reverted to the upstream behaviour: only substitute `0xad` when the module left the byte at 0. Pinning a header is now an explicit, validated diagnostic (B7). |
| B7 | med | helper, OTA version override | The override read a **fixed, world-writable path** (`/tmp/aptx_ota_version`): any local user could flip the version byte of the audio stream, and the file outlived its experiment. | Path now comes from `APTX_OTA_VERSION_FILE` (never a fixed location); only `0xad/0xae/0xaf` are accepted, anything else is ignored with a message. |
| B8 | med | plugin, `spawn_helper()` | The helper inherited the parent's **blocked signal mask** (PipeWire blocks SIGINT/SIGTERM) and **all inherited descriptors**. With SIGTERM blocked, `reap_helper()`'s graceful termination cannot work and every reap degrades to SIGKILL; inherited sockets keep PipeWire resources alive inside a crashed helper. | The child clears the signal mask, resets SIGPIPE to default and closes descriptors ≥ 3 before `exec`. |
| B9 | low | plugin, `codec_encode()` | A helper reply larger than the packet buffer was dropped **silently** (`response_size > sizeof(packet)` → success with no data), hiding a protocol bug. | Logged as a warning before the drop. |
| B10 | low | plugin, init/`codec_encode()` | The transport MTU was never sanity-checked. An MTU below what an Adaptive packet needs makes *every* packet fail with `-EMSGSIZE`, which looks exactly like "the encoder produces nothing". | Init warns when `mtu < APTX_ADAPTIVE_MIN_MTU` (700), and each oversized packet is logged before being dropped. |
| B11 | low | plugin, `helper_control()` | Control payloads were bounded only by `UINT32_MAX`. | Bounded by `APTX_ADAPTIVE_MAX_HELPER_REQUEST` (1 MiB). |
| B12 | med | runtime bundle | The deployed helper was `aptx-lossless-helper-exp`, built from `/tmp/helper_ttpaudio.c`, **whose source no longer existed** — the runtime behaviour was not reproducible from the repository. | The env-gated knobs were folded into `research/aptx-adaptive-qemu/aptx-lossless-helper.c` (inert by default), the helper was rebuilt from that source, installed as the canonical `helper/aptx-lossless-helper`, and the `-exp` binary plus its drop-in override were removed. |
| B13 | med | NixOS module | `adaptiveEnv` shipped `APTX_ADAPTIVE_STRIP_OTA=1` (strips the OTA header = the known-broken wire format), `APTX_ADAPTIVE_CAPTURE=/tmp/aptx-adaptive-reference.bin` (dumps the stream to /tmp) and `APTX_ADAPTIVE_FORCE_RATE=44100` (resamples every 48 kHz source) for **every** session, including plain aptX HD use. | Removed from the module; the comment now states that one-shot diagnostics belong in user drop-ins. |
| B14 | med | NixOS module | The same variables were exported system-wide through `environment.variables`, leaking the encoder configuration (and its diagnostic switches) into every process on the machine. | Only `systemd.user.services.{pipewire,wireplumber}.environment` receive them. |
| B15 | low | plugin, `codec_get_delay()` | Reported 0 samples (7 while downsampling), so the node latency ignored the encoder's block delay. | Reports one codec block plus the half-band filter delay, in samples — the same "one frame" convention LDAC uses (verified: `media-sink.c` converts samples → ns with the graph rate). |
| B16 | low | NixOS module comment | The comment claimed `bluez5.codecs` order decides codec priority. It does not: the array is only a whitelist, priority comes from `codec_order()`, where aptX Adaptive ranks **above** aptX HD. | Comment corrected, plus instructions to delete `aptx_adaptive` from the whitelist to make the (currently silent) codec opt-in only. |

## 2. Residual risks (accepted, documented, not fixable in software)

| # | Risk | Why it stays | Mitigation |
| --- | --- | --- | --- |
| R1 | **W^X is disabled** for `pipewire.service` and `wireplumber.service` while the module is enabled | QEMU TCG must JIT, so the codec cannot run under `MemoryDenyWriteExecute=yes`; the hardening flag is static per unit, so it is off even when aptX HD is in use | Documented in the module. `services.pipewire.aptxAdaptive.enable = false` restores the protection; do that when not experimenting. |
| R2 | Proprietary extracted blob (`aptx_adaptive_enc_module.so.1`, Hexagon) | The only aptX Adaptive encoder in existence; no open implementation | Never enters the Nix store; `README.md`/`STATUS.md` already state it must not be used as a general audio backend without a licence and real-time review. |
| R3 | **ABR is non-functional** with this encoder build: the stream is fixed-rate (~210 kbps at 25 ms) | The R2.2 CAPI build ignores IMCL quality-level feedback outside a full AudioReach container; no controller-side RF/BER feedback exists on the AX210 | Plugin logs it at init; the review of the user's own comparison document records the same conclusion. |
| R4 | TTP is host-synthesized | The wrapper only advances TTP on sink-side clock feedback, which this bridge never receives | Default is now the **audio clock** (`TTP = samples/rate*15000 + offset`), which is at least self-consistent with the RTP timestamps we generate; `APTX_TTP_MODE=wall` reproduces the historical wall-clock value for A/B tests. |
| R5 | R3/Lossless is incomplete (cursor guard returns `-EOVERFLOW` after ~94 calls; a forced Lossless request is downgraded unless `APTX_ADAPTIVE_ALLOW_UNSTABLE_LOSSLESS=1`) | The R3 kernel needs the missing controller-side feedback | Refuses cleanly instead of crashing; documented in `HANDOFF.md` §16. |
| R6 | The sink stays silent with a byte-identical stream | Unresolved; the only remaining difference is the controller/air interface (see `HANDOFF.md` §18/§19) | This is exactly what the Ubertooth capture is for. |

## 3. Verification performed in this round

1. Helper rebuilt from the repository source with the project's own `compat.c`
   (log gate intact) and installed as the canonical runtime binary.
2. Helper regression, plain R2 @44.1 kHz: `664 B` packets, OTA
   `… 64 01 00 00 00 ae`, frame header `8300c0a1`, one packet per 25 ms block,
   TTP advancing 375 units per packet from an audio-clock base (3900 =
   the sink's 260 ms `DELAY_REPORT`).
3. Helper knobs: `APTX_TTP_MODE=wall` (old behaviour), `APTX_OTA_VERSION=0xaf`
   (applied) and `=0x99` (rejected), `APTX_OTA_VERSION_FILE=<path>` (applied),
   `APTX_FORCE_BITS=16` (switches to the 768-byte R2.2 shape),
   `mode=r3` (config now succeeds).
4. Plugin: compiled clean before and after the edits — identical diagnostics,
   i.e. no new errors or warnings from the changes.
5. Runtime: the only remaining drop-in is `zzz-aptx-phone-exact.conf`
   (`SOURCE_TYPE=0x00`, `CHANNEL_MODE=stereo`; `FORCE_RATE` commented out,
   `FEATURES` unset), verified against `/proc/<wireplumber>/environ`.

## 4. Known-open items (not bugs, tracked for the next rounds)

- The sink-silence question itself (see the Ubertooth plan in `HANDOFF.md` §19).
- `helper/eap`-style end-to-end tests do not exist for the plugin; only the
  helper has probe tooling (`helper_probe.py`, `raw_probe.py`).
- The plugin is deployed from a fork revision; every plugin change needs a
  `nixos-rebuild`. Helper-only changes do not.
