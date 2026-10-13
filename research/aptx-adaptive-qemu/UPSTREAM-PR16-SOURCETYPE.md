Thanks for the recipe. The source-type observation is the most useful lead this
thread has had, so I spent today testing it end to end on a different sink. Short
version: on an R2.2 sink it does open the audio path, but the link dies within a
second or two. Details below in case they help you or anyone else.

Setup: Sennheiser MOMENTUM 5 (an R2.2 sink) from an Intel AX210, stock BlueZ
5.87 and PipeWire 1.7.0 with our bridge, which runs the Qualcomm CAPI encoder
under qemu-hexagon. The AVDTP configuration we send is
byte-identical to what a Snapdragon phone sends to this headset.

Your three points, as environment switches (all off by default, so the deployed
wire format is unchanged):

- `APTX_ADAPTIVE_SOURCE_TYPE` (already existed): the low bits of config byte 6,
  `0x00` or `0x02`.
- `APTX_ADAPTIVE_TS_CONSTANT` (new): the RTP timestamp becomes the packet's
  samples per channel (1200 for our 25 ms / 48 kHz frames), high 16 bits zero.
- `APTX_ADAPTIVE_SSRC` (new): 0 by default, which is what the phone sends; 0xA3
  for HQ.

Your point 3 we already satisfied: our configuration carries 0x64 in the TTP
byte.

What we measured (btmon, counting `< ACL: Handle 256`; encoder throughput from the
helper's `wchar` counter):

| configuration | encoder | air | heard |
| --- | --- | --- | --- |
| source type 0 (baseline) | 26.9 kB/s (40 pkt/s) | 27.3 kB/s (40 pkt/s), sustained | dead silence |
| source type 2 + constant timestamp | 26.9 kB/s | 19 pkt/s for the first second, then 1-2 pkt/s | brief pops, no audio |
| the same plus SSRC=0xA3 | 26.9 kB/s | 40 pkt/s for the first second, then collapse | no audio |

So source type 2 does get the sink decoding: we hear short, meaningless pops,
which source type 0 never produces. But the link collapses one to two seconds in.
During the collapsed phase a 10 s capture shows **zero** ACL packets on that
handle while the controller still reports `Number of Completed Packets` events,
i.e. the baseband stopped being acknowledged and the controller's queue is stuck.
The encoder never stops producing.

I also tried to change the packet period, since your HQ example is 672 samples
(14 ms) and ours is 25 ms / 676 bytes:

- `APTX_ADAPTIVE_CAP_EXT_VER=0` (new; forces a plain-R2 negotiation both in what
  we advertise and in the negotiated result): the frame on the air stayed 680
  bytes, i.e. still 25 ms.
- AVDTP channel mode `joint` instead of `stereo`: same.

So the period hypothesis is untested rather than disproved: in this build the
period appears fixed at 25 ms whatever we do to ext-ver or channel mode.

Two questions if you still have your notes. What did your Px7 advertise for
`cap_ext_ver_num`, and what packet period did your stream use on the air? And when
you saw the sink "taking data at about a third of real time", did it ever collapse
completely, or did the constant timestamp cure the rate for you? If your sink is
R1 and this one is R2.2, the difference may be the wrapper state we cannot feed:
our R2.2 path waits for QHS/16-bit sideband feedback that a non-Qualcomm
controller cannot provide, and with Lossless disabled we deliberately clear that
capability bit so the wrapper does not enter that state.

Raw data and write-ups: research/aptx-adaptive-qemu/STATUS.md sections 5, 6 and
7.1-7.5, HANDOFF.md sections 21.34-21.38, branch research/aptx-adaptive-final.
