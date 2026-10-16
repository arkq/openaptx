# ESP32-SDR check: the phone's Adaptive link stays invisible

Following the ESP32-as-SDR suggestion in this thread, I ran the measurement
with an ESP32-C5 running the ESP-SDR (ESPARGOS) raw-IQ firmware. It receives
2.4 and 5 GHz
without decoding any protocol, which removes the address-recognition
assumption my
earlier sniffer result rested on, and it let me add the control that matters: the
same phone, at the same distance, playing the same track, with only the Bluetooth
codec changed.

**Method.** Burst captures of 819 us at 20 MSa/s with an 11 MHz analog filter and
a fixed manual gain, 200-300 captures per leg, at 2441 MHz for 2.4 GHz. Two
statistics per capture: the rise of a 1 MHz band over its own 8 MHz neighbourhood
("burst", a narrowband detector), and DC-removed power. A power-domain mean
spectrum is accumulated as well, since it responds to wideband energy too.

## 2.4 GHz, phone at 30-50 cm, only the codec changed

| state | captures with a burst >6 dB |
| --- | --- |
| phone, SBC (standard EDR) | 43/200 = 21.5 % |
| phone, AAC 44.1 kHz (standard EDR) | 23/200 = 11.5 % |
| this host, aptX HD at ~0.5 m (standard EDR) | 32/200 = 16.0 % |
| phone, aptX Adaptive | 6/200 = 3.0 % (passes: 6.5, 9.5, 10.5, 4.3 %) |
| silence | 5/200 = 2.5 % (repeat: 2.5 %, 6.7 %, 1.3 %) |

The Adaptive passes scatter and overlap the silence passes, which is what a null
looks like once the room is allowed to drift; the standard-EDR rows sit clearly
above both. In a strictly paired run, taken back to back in one sitting with the
phone untouched, Adaptive read 4.3 % playing against 6.7 % paused.

**One pitfall worth flagging.** Raw RMS includes the front end's DC/leakage term,
and that term moves when a nearby transmitter is active, so it can look like signal
where there is none. With DC removed, a paired run gives:

| metric | Adaptive playing | paused |
| --- | --- | --- |
| burst >6 dB | 7/200 = 3.5 % | 8/200 = 4.0 % |
| raw RMS | -37.0 dB | -42.4 dB |
| DC-removed power | **-44.6 dB** | **-44.4 dB** |
| mean power spectrum, middle 16 MHz | **+0.36 dB**, no structure | - |

The mean power spectrum matters here: a *wideband* emission raises it just as a
narrowband one raises a few bins, and it shows nothing. So within this window
Adaptive playback is not distinguishable from silence in burst rate, in
DC-removed power, or in spectral shape.

**5 GHz.** This branch needed its own control, because the narrowband detector is
blind to Wi-Fi-shaped energy. This host's Wi-Fi was associated on channel 44
(5220 MHz) a metre from the receiver; a sustained download moved that window from
-55.1 dB to -45.6 dB (+9.5 dB median, +12.8 dB at p90) and raised the mean power
spectrum by +12.4 dB, so an emission of that order at that range would have been
seen. During Adaptive playback, six 20 MHz windows across 5180-5825 MHz showed
nothing: 0.0 % bursts at every centre, power differences against idle between
+0.4 and -5.7 dB with the playing legs *quieter*, and a strictly paired repeat at
5500 and 5745 MHz disagreeing in sign (-4.3 dB and +1.9 dB). About 120 MHz of the
band was covered rather than all of it, and the headset has no 5 GHz radio to
begin with, so this closes a low-probability branch rather than a live one.

**On transmitting.** ESP-SDR is receive-only by design ("we are not providing a
transmitter implementation at this time"), and the ESP32 transmit work that does
exist is narrowband polar modulation (FM/SSB), not an arbitrary wideband waveform,
so an ESP32 cannot stand in for the vendor silicon that appears to be required at
the source end. What it can do, cheaply, is show what is actually on the air,
which is what I used it for.

**Limits.** The burst detector is deliberately narrowband (the mean power spectrum
covers the wideband case). One receive centre at 2.4 GHz, 819 us windows, so
roughly 20 of the 79 channels per window -- though standard-EDR codecs from the
same phone were visible in that same window, so the phone's channel map does
include it. At centimetre range the front end compresses and even SBC reads only
5 %, so comparisons have to be taken at 30-50 cm and in pairs, since the ambient
band itself drifts between roughly 1 % and 7 %.

Raw data, scripts and the write-ups: research/aptx-adaptive-qemu/STATUS.md sections
7.1-7.5 and HANDOFF.md sections 21.29-21.33, branch research/aptx-adaptive-final.
