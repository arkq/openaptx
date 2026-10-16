# Follow-up: what Windows actually does about aptX Adaptive

Prompted by the link to Microsoft's Bluetooth Classic audio page, we unpacked both
Windows 11 25H2 ISOs (x86_64 and ARM64) to find out what "aptX Adaptive is only
supported on select Windows devices with compatible Qualcomm Bluetooth radios"
means in practice. The short version: **Windows ships the framework, not the codec,
and the framework needs hardware a PC Bluetooth card does not have.** It is a third,
independent confirmation of the conclusion in the previous comment -- this
time from
Microsoft's own binaries and documentation rather than from Qualcomm's.

## What is in the images

- x86_64: 1 078 907 file entries; ARM64: 521 031. **Zero files with "aptx" in the
  name** in either image. The only Qualcomm entries are Wi-Fi FOD packages and
  `plutonfw_Qualcomm.dll` (security firmware).
- The A2DP profile driver `BthA2dp.sys` (x64, 602 112 bytes, DriverVer
  10.0.26100.7920) carries the vocabulary of the feature, including what it does
  *not* do:

```text
AptX Adaptive sideband codec does not support codec operations in profile driver.
AptX Adaptive sideband codec does not support decoding in profile driver
aptX adaptive sideband codec is not available for sink role
AptXAdaptiveLatencyMode / A2dpAptXAdaptiveLatencyModeChange / AptXAdaptiveNewSamplingHz
UpdateMtuSizeSiopWorker
A2dpAvailableOffloadedCodec / IsA2dpDeviceOffloaded / A2dpOffloadDisabled
AVDTP Offload : Invalid AVDTP codec Category reported by the controller
Failed to create inband AAC codec / Failed to create inband SBC codec
```

SBC, AAC and aptX have both an "inband" form (implemented in the
profile driver) and a "Sideband" form (implemented in the radio).
**aptX Adaptive appears only in the sideband form**, and the driver's
import table lists no codec library at all (`ntoskrnl`, `HAL`,
`ks.sys`, `btampm.sys`, `WppRecorder`, `WDFLDR`, `ksecdd`).
The ARM64 driver carries identical strings. `BtaMPM.sys` only coordinates media
state (`BtaMpmUpdatePlayStatus` / `BtaMpmUpdateSuspendStatus`).

## What Microsoft documents

[Audio Sideband A2DP Offload](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/audio-sideband-a2dp-offload)
(Windows 11 build 10.0.22000 and later) describes the mechanism:

- Audio reaches the controller "through an alternate path rather than the normal
  controller interface (the HCI)". That path is "often an I2S or PCM
  interface, but
  can also be a richer, more complex bus such as SLIMbus ... or potentially
  SoundWire".
- The codec "is implemented in the audio DSP and/or Bluetooth
  controller"; if it is in the audio DSP, the controller "shall be
  able to extract the encoded frames from the data transmitted
  across the audio transport to package these frames into AVDTP
  media packets".
- The host-to-controller protocol is a set of Microsoft-defined vendor HCI commands,
  `HCI_VS_MSFT_Avdtp_*`, gated by a bit in
  [`HCI_VS_MSFT_Read_Supported_Features`](https://learn.microsoft.com/en-us/windows-hardware/drivers/bluetooth/microsoft-defined-bluetooth-hci-commands-and-events)
  (`0x00000000 00000080`); the vendor-chosen base opcode is published to Windows
  through the registry value `VsMsftOpCode`.
- The audio driver must publish
  `GUID_SIDEBANDAUDIO_A2DP_SUPPORT_INTERFACE`; codec and transport parameters travel
  as SIOPs, and the codec-latency-mode and L2CAP-MTU SIOPs are, in Microsoft's
  words, "only used when the aptX Adaptive codec is active".
- Microsoft states plainly: "A2DP offload is not defined in any Bluetooth SIG
  standard."

The pattern is the same as the older HFP/SCO bypass: in
[HFP bypass guidelines](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/bluetooth-bypass-guidelines-for-audio-drivers)
Microsoft describes routing audio "via I2S or another connection between the audio
codec and the Bluetooth radio, bypassing the Bluetooth HCI".

## The flow

All but the first share the `HCI_VS_MSFT_Avdtp_` prefix.

| Subcommand (value) | Key parameters | Returns |
| --- | --- | --- |
| `Read_Supported_Features` (0x00) | -- | bitmap, bit 0x80 |
| `Capabilities_Configuration` (0x07) | codecs, params | controller codecs |
| `Open` (0x08) | handle, L2CAP CID, MTU, codec | offload handle |
| `Start` (0x09) | offload handle | status |
| `Suspend` (0x0A) | offload handle | status |
| `Close` (0x0B) | offload handle | status |

1. The radio driver publishes `VsMsftOpCode`; Windows reads the feature
   bitmap; the
   IHV audio driver publishes the sideband interface.
2. Codec capabilities are exchanged: the profile driver sends the audio side's
   codecs to the controller and receives the controller's list. If both
   provide a
   list, **Windows uses only the controller's**.
3. Normal AVDTP negotiation selects the codec and opens the L2CAP media
   channel. The profile driver already knows whether it is sideband.
4. `IOCTL_SBAUD_STREAM_OPEN` triggers `Avdtp_Open`, which allocates the offload
   resource; the controller returns the handle plus the real MTU and latency mode.
5. `IOCTL_SBAUD_STREAM_START` triggers `Avdtp_Start`. The controller then receives
   audio from the sideband transport, encodes it (or extracts already-encoded frames
   from the transport), assembles AVDTP payloads and **transmits the media itself**.
   The media never crosses HCI.

This is structurally identical to what we measured on an Android phone: no L2CAP
media packets in its HCI log at all, and a single vendor command carrying the A2DP
configuration right after AVDTP Start. Only the command set differs per OS.

## The hardware conditions

1. a controller that implements the `HCI_VS_MSFT_Avdtp_*` commands and advertises
   the offload bit;
2. the aptX Adaptive codec present in the controller or in an audio DSP;
3. an IHV audio driver publishing the sideband A2DP interface;
4. **a physical sideband audio path between the audio DSP and the Bluetooth
   controller** (I2S/PCM/SLIMbus/SoundWire);
5. Windows 11 22000+ for the framework, 24H2+ for the Adaptive entry.

Condition 4 is the one that matters here: an M.2 card in a laptop has no such wiring,
and Microsoft's own answer for the NFA765 (FastConnect 6900) is that the chip "does
not have the aptX Adaptive codec"
([Q&A](https://learn.microsoft.com/zh-cn/answers/questions/4010192/nfa765-win11-aptx-adaptive)).

## What it means for this project

- Windows is a third independent confirmation that the audio is produced below the
  host stack; the host does AVDTP signalling and a little control, nothing more.
- Windows is **not** a source of a portable aptX Adaptive encoder. There is nothing
  in the images to port, and the documented path cannot be driven from a host codec
  plugin.
- On this class of hardware, Linux and Windows end up in the same position: the
  missing piece is vendor silicon plus a sideband audio path, not a better bitstream.
  Our own measurements agree -- a byte-identical stream, and the headset still
  negotiates Adaptive and stays silent.

If anyone has a first-hand case of aptX Adaptive audio from a non-Qualcomm Bluetooth
controller, or of a retail M.2 module exposing this offload path, that would change
the picture and we would like to hear about it.
