# Update: evidence on "is this a dead end?"

Answer: the host-side half is verified and is not the problem. The stream is
nevertheless not audible on the headset we tested, and the evidence now points
to a vendor audio path below the host stack rather than to the bitstream. What
follows separates measurement from inference.

## What is verified

On an Intel AX210 (a non-Qualcomm controller), this branch produces an aptX
Adaptive R2 stream that is correct on the wire:

- AVDTP SET_CONFIG is byte-identical to a shipping Snapdragon source:
  `d7 00 00 00 ad 00 40 02 50 64 64 64 ff ff 00 01 92 00 00 0f 02 03 03 03 00 aa`
- The RTP header has the same structure as aptX HD (`80 60 <seq> <ts> 00000000`),
  and the 8-byte OTA header
  (`[TTP:2][period:1][packet_type:1][channel_mode:1][pad:2][version:1]`) matches
  the phone's except for TTP.
- Measured from our own HCI: a 676-byte L2CAP payload every 25.00 ms carrying
  656-byte codec frames, version `0xae`. The period byte is in 0.25 ms units and
  TTP in 1/15000 s units, and the measured cadence is cross-checked against the
  period field the stream itself declares.
- Encoding uses Qualcomm's Hexagon CAPI module under `qemu-hexagon`. The R2.2
  "Snapdragon Sound" form (version `0xaf`, ptype 5, 760-byte frames,
  channel_mode `0xa0`) was emitted continuously as well.

The parser and tests added in this PR describe the R2 form and are unaffected
by the findings below.

## Why it stays silent

1. The headset's own app reports "aptX Adaptive 48 kHz" while the stream plays
   silently, so negotiation succeeds.
2. Not a gain gate: the A2DP transport is `active` at `Volume = 63/127`, and
   maximum headset volume produces no hiss at all.
3. Not digital silence: digital silence encodes to a single repeated frame
   (median byte entropy 0.33), while pink noise and a tone produce all-distinct
   frames (entropy ≈6.8).
4. Not a bitstream bug: replaying the Android source's own captured frames,
   with a clean monotonic TTP, is silent too.
5. On the air: with a phone held against the antenna and audibly streaming
   Adaptive, a 79-channel survey (it sweeps every channel, so a follower's
   missing AFH map is irrelevant) found **zero** packets for the phone's
   address over **307 s**. What makes that meaningful is the in-situ control:
   the same instrument, same room, same headset, sees this host's aptX HD
   stream at 0.7-1.6 hits/s, this host's own Adaptive stream at 0.52 hits/s,
   and only 0.02 hits/s when that link is idle -- so the visibility is
   media-driven, and at the control's rate the phone should have produced
   215-460 hits. An independent single-address mode also saw zero. The
   remaining limit: the Ubertooth cannot demodulate EDR payloads, so the claim
   is "no access code was detected", not "we know what was sent instead"; a
   2.4 GHz SDR would settle that part.

## Mechanism found

In an Android HCI log of a Snapdragon source, the entire session contains **no
L2CAP media packets at all**. Immediately after AVDTP Start there is a single
vendor command (`0xFC0A`, 66-byte payload) carrying the A2DP configuration, and
the host never sends Adaptive media for the rest of the session. The controller
encodes on-chip.

Qualcomm's Snapdragon Sound whitepaper names a proprietary link technology:
"a 4dB gain using Qualcomm High Speed Link modulation and a further 2dB gain
using Qualcomm aptX Adaptive. Fewer retries and less time on the radio ...
Advanced modulation and coding also help to deliver increased end-to-end
Bluetooth link robustness", and the trademark page of the same document lists
"Qualcomm High Speed Link" as a Qualcomm product.

**Documented:** that Qualcomm ships a proprietary link modulation under that
name; that AOSP's standardised A2DP-offload codec list contains no Adaptive
bit; that mainline BlueZ and upstream PipeWire have no aptX Adaptive codec id.
**Inferred, not documented:** that this modulation is what carries Adaptive
audio, or that Adaptive *requires* it. Two reasons to keep that inference
tentative: the whitepaper presents the feature as a robustness and range gain
rather than as a media path; and Qualcomm quotes Adaptive at 279 to 420 kbps,
which fits inside standard EDR without difficulty, so a proprietary modulation
is not needed *for rate*. Note as well that "QHS" is community shorthand -- the
acronym does not appear in Qualcomm's document.

One limit of the air measurement, so that it is not overread: the Ubertooth One
documents Basic Rate and BLE capture only, and cannot decode EDR payloads. An
EDR link's access code and header are still GFSK, so survey detection still
sees such a link; the zero counts are meaningful, but they count detected
packets, not decoded audio.

## Value to upstream

The protocol facts above; two cheap tools — wire-level verification of a stream
against what it declares, and a frame-entropy check that distinguishes real
content from digital silence; and the negative result itself. We found no
first-hand report of aptX Adaptive audio from a non-Qualcomm controller to a
Snapdragon Sound headset on any OS. If such a report exists, it would change
this conclusion, and we would like to hear about it.
