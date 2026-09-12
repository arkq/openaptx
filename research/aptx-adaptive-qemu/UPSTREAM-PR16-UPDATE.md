## Update: evidence on "is this a dead end?"

Answer: the host-side half is verified and is not the problem. The stream is
nevertheless not audible on the headset we tested, and the evidence now points
to a vendor audio path below the host stack rather than to the bitstream. What
follows separates measurement from inference.

### What is verified

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

### Why it stays silent

1. The headset's own app reports "aptX Adaptive 48 kHz" while the stream plays
   silently, so negotiation succeeds.
2. Not a gain gate: the A2DP transport is `active` at `Volume = 63/127`, and
   maximum headset volume produces no hiss at all.
3. Not digital silence: digital silence encodes to a single repeated frame
   (median byte entropy 0.33), while pink noise and a tone produce all-distinct
   frames (entropy ≈6.8).
4. Not a bitstream bug: replaying the Android source's own captured frames,
   with a clean monotonic TTP, is silent too.
5. On the air (Ubertooth One): every source that *plays* — a Snapdragon phone,
   and the FiiO BT11 in both Adaptive and Lossless — is invisible on standard
   BR/EDR in steady state: 259 hits during a 31 s reconnection burst, then 0
   hits for 118 s while audio plays. The only steady-state source visible on
   standard EDR is this host, and it is the silent one. Steady-state negatives
   total 488 s (Adaptive) and 551 s (Lossless); a 304 s control capture
   confirmed the instrument was live (535 packets, 32 piconets). An earlier
   reading that Lossless ran on plain EDR was withdrawn after the captures were
   re-anchored.

### Mechanism found

In an Android HCI log of a Snapdragon source, the entire session contains **no
L2CAP media packets at all**. Immediately after AVDTP Start there is a single
vendor command (`0xFC0A`, 66-byte payload) carrying the A2DP configuration.
The controller encodes on-chip. Qualcomm's Snapdragon Sound material documents
"Qualcomm High Speed Link modulation" (4 dB link-budget gain, "advanced
modulation and coding techniques", fewer retransmissions) — a modulation-level
change, consistent with a standard BR/EDR sniffer observing nothing.

**Documented:** High Speed Link modulation; AOSP's standardised A2DP-offload
codec list, which has no Adaptive bit; mainline BlueZ and upstream PipeWire
having no aptX Adaptive codec id. **Inferred, not documented:** that aptX
Adaptive *requires* High Speed Link.

### Value to upstream

The protocol facts above; two cheap tools — wire-level verification of a stream
against what it declares, and a frame-entropy check that distinguishes real
content from digital silence; and the negative result itself. We found no
first-hand report of aptX Adaptive audio from a non-Qualcomm controller to a
Snapdragon Sound headset on any OS. If such a report exists, it would change
this conclusion, and we would like to hear about it.
