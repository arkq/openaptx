# aptX Adaptive on non-Qualcomm hardware: measured data

Adding measured data rather than a feature request.

**A working host-side implementation exists out-of-tree.** A PipeWire fork
built with `-Dbluez5-codec-aptx-adaptive=enabled`, driving a proprietary
Qualcomm Hexagon encoder module under `qemu-hexagon`, produces a stream that
verifies correctly on the wire from an Intel AX210: AVDTP SET_CONFIG
byte-identical to a shipping Snapdragon source, a 676-byte L2CAP payload every
25.00 ms carrying 656-byte frames with version `0xae`, and an RTP/OTA header
matching the phone's except for TTP.

**The blocker is not the encoder.** That same stream is rejected by at least
one commercial headset (Sennheiser MOMENTUM 5: completely silent) while aptX HD
plays over the same link from the same controller, and while the headset's own
app reports "aptX Adaptive 48 kHz". It is not a gain gate
(`Volume = 63/127`, transport `active`) and not digital silence: known silence
encodes to one repeated frame (entropy 0.33), pink noise and a tone to
all-distinct frames (entropy ≈6.8). Replaying the Android source's own captured
frames is silent too. A within-subject air capture shows a reproducible
contrast (connection setup visible, media phase not), though the instrument's
limits -- it never applies the AFH channel map, and passive capture of even a
standard A2DP link recovers a small fraction of the packets -- make that
suggestive rather than proof that the audio leaves the standard PHY.

**Why a codec module alone cannot go further.** Mainline BlueZ has no aptX
Adaptive codec id; mainline Linux has no A2DP offload at all (`hci_qca` carries
only HFP voice offload); and the transport shipping Qualcomm sources actually
use is controller-side: in an Android HCI log the session contains no L2CAP
media packets, and a single vendor command (`0xFC0A`, 66-byte payload) carries
the A2DP configuration right after AVDTP Start. Qualcomm's own Snapdragon Sound
whitepaper additionally names a proprietary "Qualcomm High Speed Link"
modulation, presented there as a robustness and range feature (the document
does not say it carries the audio, and Adaptive's 279-420 kbps fits standard
EDR). None of that is reachable from a host codec plugin.

**What would make this useful upstream** is any non-Qualcomm controller that a
headset will actually decode — i.e. a data point where aptX Adaptive audio is
audible from Intel, MediaTek, Realtek, Broadcom or CSR silicon. We have found
no such first-hand report on any OS. If anyone here has one, it would directly
change the conclusion above; we would rather be corrected than keep a wrong
negative result.
