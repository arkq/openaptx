# Solved on an R2.2 sink: drop the extra 8-byte container header

Short version: on the Sennheiser MOMENTUM 5 (R2.2) the stream now plays, and the
cause of the year-long silence was not the codec, the link or the sink firmware
but the A2DP payload we assembled: we prefixed the 656-byte codec packet with the
Qualcomm CAPI's 8-byte container header (TTP, period, cfg, version). That header
belongs to the controller *offload* framing. A host-side encoder must send the
codec packet directly after the 12-byte RTP header.

Credit where it is due: this fell out of reading theacrat's published
implementation and specs (<https://github.com/theacrat/aptx-linux>,
`spec/a2dp-integration.md` §7D/§7F, "Every media packet is 668 bytes: a 12-byte
RTP header followed directly by the 656-byte codec packet. There is no separate
8-byte header in the payload."), which is also consistent with the Qualcomm ADK's
`APTX_AD_DEFAULT_PACKET_SIZE 668`.

What was tested (Intel AX210, stock BlueZ 5.87, PipeWire 1.7.0, Qualcomm CAPI
encoder under qemu-hexagon, MOMENTUM 5):

| payload on air | L2CAP payload | air rate | result |
| --- | --- | --- | --- |
| RTP + container + 656-byte frame | 676 B | stalls after 1 s | silence |
| RTP + 656-byte frame | 668 B | 40.0 pkt/s for 60.8 s, 26.9 kB/s | **plays** |

The stripped form is the only change; the encoder, the negotiated configuration
and the rest of the packet are the same. Everything else I reported earlier
(replays of the phone's own frames, a configuration element byte-identical to a
Snapdragon phone's, SSRC variants, constant vs running timestamps, TTP base, the
sink's own latency octets, marker bit) left the one-second collapse untouched —
which now makes sense: the sink never got past the container header.

Working configuration on this sink:

- codec config byte 6: `0x12` (48 kHz, source type 2 — the host/NQ2Q mode)
- RTP timestamp low 16 bits: samples per channel in the packet (1200 = 25 ms at
  48 kHz), constant; upper 16 bits zero
- SSRC: `0x000000A3` (HQ); `0xA1`/`0xA2` also sustain the link here
- payload: 656-byte codec packet, no container header (668-byte L2CAP payload)

On the "matching the phone" argument in my earlier comment: the phone's Adaptive
media never appears on HCI because it is offloaded, so what we matched was the
offload framing, not the host framing. That was the wrong reference.

Limits of this result: our encoder build pins the packet period at 25 ms /
204 kbps, i.e. R2 HQ level 2 in theacrat's table; the higher HQ levels
(270k/19 ms, 340k/15 ms, 427k/12 ms) and the low-latency rows (220k/12 ms,
270k/10 ms) would need a host encoder that can choose its period.
