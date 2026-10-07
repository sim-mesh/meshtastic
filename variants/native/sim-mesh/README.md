# Meshtastic on sim-mesh

meshtasticd as a [sim-mesh](https://github.com/sim-mesh/sim-mesh) station: the
Linux daemon (Portduino), its SX1262 and its pins on sim-mesh's virtual chip
(sim-mesh's `radio/portduino`) instead of spidev and libgpiod, with its API
client as a second, radio-less process of the station.

| Base | What | The station's console |
|---|---|---|
| `meshtastic-sx1262-<release>` | meshtasticd, `[env:sim-mesh]` | `host.py`'s command line, and framed RPC for sim-mesh's driver |

```
station console pty ◄──► host.py ── TCP <bind addr>:4403, ToRadio/FromRadio ──► meshtasticd (SX1262)
sim-mesh ── framed RPC on the console ──► host.py ── a command ──► the API
host.py ── "mthost: {json}" lines ──► the driver (category meshtastic)
meshtasticd ── RadioLib ──► sim-mesh radio/portduino ──► the ether
```

## The pieces

- `platformio.ini`: `[env:sim-mesh]`, meshtasticd without Linux hardware,
  UDP multicast, the screen or the web server, linked against sim-mesh's
  `radio/portduino` from sim-mesh's clone beside this repository, and with
  `-Wl,--wrap=bind,listen,accept,close` so the API's sockets are part of the
  station's idle.
- Under `SIM_MESH` in the source: `native_radio_backend_init()`, a weak hook
  before `SPI.begin()`, installs the simulated chip; `InterruptableDelay`
  idles in sim-mesh's idle and wakes on its wake, and an idle that ended on
  a socket makes the API server's threads due at once; RadioLib's `yield()`
  is one millisecond of the radio's idle, and a sub-millisecond
  `delayMicroseconds()` takes no time; the RTC is set at NTP quality from the
  run's clock; `Power::reboot` exits, so the station restarts whole.
- `sim/host.py`: the station's host. It starts meshtasticd, connects to its
  TCP API once, answers sim-mesh's framed RPC with its commands in JSON,
  gives a person at the console a command line, and prints
  `mthost: {json}` lines for every packet and acknowledgement. Settings go in
  one transaction, since meshtasticd reboots after each. In a virtual-time
  run it joins the ether as a station of its own, with no radio.
- `sim/driver.py`: the sim-mesh driver, category `meshtastic`.
- `sim/make-zips`: builds `[env:sim-mesh]` for aarch64 and x86_64 and makes
  the firmware zips.
- `sim/test_host.py`, `sim/test_driver.py`: the host against a fake
  meshtasticd, and the driver against a stand-in station.

## Building

PlatformIO, and sim-mesh's clone beside this repository (`../sim-mesh`,
whose `radio/build/` holds `libsimradio-sx1262.so` once sim-mesh has
started, or `cmake -S ../sim-mesh/radio -B ../sim-mesh/radio/build && cmake
--build ../sim-mesh/radio/build`). Portduino needs the headers of libuv,
i2c-tools, libgpiod, yaml-cpp, libbsd and OpenSSL (`libuv1-dev libi2c-dev
libgpiod-dev libyaml-cpp-dev libbsd-dev libssl-dev` on Ubuntu).

```sh
pio run -e sim-mesh                          # .pio/build/sim-mesh/meshtasticd
variants/native/sim-mesh/sim/make-zips       # both architectures, in .pio/sim-zips/
```

**Both architectures.** sim-mesh's pre-built firmware carries aarch64 and
x86_64, so `make-zips` builds for both (`--arch` for one): the machine's own
natively in `.pio/build/`, the other in `.pio/build.linux-<arch>/` with that
architecture's cross g++ (`g++-x86-64-linux-gnu` or
`g++-aarch64-linux-gnu`). `SIM_MESH_ARCH=<arch>` in pio's environment
selects it: sim-mesh's `radio/portduino/cross.py` swaps in the cross tools,
and its `link.py` compiles the radio with them and links that copy. By hand:

```sh
SIM_MESH_ARCH=x86_64 PLATFORMIO_BUILD_DIR=.pio/build.linux-x86_64 pio run -e sim-mesh
```

meshtasticd loads libraries beyond the C library and the C++ runtime
(yaml-cpp, libusb, libi2c and theirs), so `make-zips` puts them in the
zip's `lib/`, from each architecture's multiarch directory: for every
architecture it builds for, that architecture's `libyaml-cpp-dev`,
`libuv1-dev`, `libi2c-dev`, `libusb-1.0-0-dev`, `libssl-dev` and
`libgpiod-dev` must be installed (`:amd64` on an aarch64 host). It fills
the zip's `pylib/` with the protobuf bindings generated from this
repository's own `protobufs` and the pure-Python protobuf runtime, fetching
`grpcio-tools` and `protobuf` from PyPI with the pip of the Python running
it. The zip's base names the release, from `version.properties`
(`meshtastic-sx1262-2.7.26_aarch64_<stamp>.zip`), and goes to sim-mesh with
`sim firmware add <zip>`.

The tests run with sim-mesh's Python environment (its `testbed/` gives
`sim_mesh.driver`); `test_host.py` builds the bindings into `sim/.pylib`
the first time:

```sh
cd variants/native/sim-mesh/sim && python3 -m pytest -q
```
