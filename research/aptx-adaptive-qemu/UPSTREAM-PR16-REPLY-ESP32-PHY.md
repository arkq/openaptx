# Reply draft (short): the ESP32-SDR route

@AndyCruz08

Thanks -- the Snapdragon-only finding matches ours: neither Windows 11 25H2
image contains any aptX component, and `BthA2dp.sys` treats aptX Adaptive as
an offloaded "sideband codec" whose audio arrives over an I2S/PCM-style bus,
not HCI.

One correction before anyone builds a transmitter: the ESP32 route is a cheap
*receiver*, not a usable transmitter. ESP-SDR gives raw IQ at 2.2-2.7 GHz,
~13-54 MHz bandwidth and up to 80 MSa/s, so a ~30 EUR board beats our
Ubertooth on bandwidth and needs no protocol assumptions. But ESPARGOS
deliberately ships no transmitter ("we are not providing a transmitter
implementation ... due to the potential for misuse"), and the ESP32 transmit
that does exist is narrowband FM/SSB via polar modulation. Emulating a
wideband vendor PHY is therefore a research project, not a plan; measurement
comes first.

Where we stand: with a phone held against the antenna and audibly streaming
aptX Adaptive to a MOMENTUM 5, a 79-channel survey saw zero packets for its
address across 307 s, while in-situ controls saw a standard-EDR aptX HD stream
at 0.7-1.6 hits/s and our own Adaptive stream at 0.52 hits/s (215-460 hits
expected had the phone emitted the same way). Something is off the standard
PHY; we do not know what it is.

Plan: four 1-2 minute captures on one board -- phone idle, phone playing
Adaptive, host playing aptX HD, host playing Adaptive -- checking whether an
emission tracks playback, how wide it is, and whether it hops like BR/EDR. A
standard-BR/EDR result would falsify our reading, and we would publish that.

Two questions: has anyone used ESP-SDR on classic Bluetooth rather than Wi-Fi
(any advice for the burst duty cycle), and is the S31 streaming profile the
only practical option for a continuous stream?

Results will be posted here either way.
