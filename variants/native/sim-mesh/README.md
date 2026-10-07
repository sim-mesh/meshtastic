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
- `sim/make-zips`: builds `[env:sim-mesh]` and makes the firmware zip, for
  the machine's own architecture or the ones asked for.
- `.github/workflows/build_sim_mesh.yml`: the zip for x86_64 and for
  aarch64, each on a runner of its own architecture.
- `sim/test_host.py`, `sim/test_driver.py`: the host against a fake
  meshtasticd, and the driver against a stand-in station.

## Building

As every native build: PlatformIO and the native build's libraries (what
`.github/actions/setup-native` installs, or the `Dockerfile`'s builder
stage), and besides them sim-mesh's clone beside this repository
(`../sim-mesh`), whose radio library the program links. `make-zips` builds
that library (`cmake`, into `../sim-mesh/radio/build/`) when the clone has
none yet, then the program for this machine's architecture, then the zip:

```sh
pio run -e sim-mesh                          # .pio/build/sim-mesh/meshtasticd
variants/native/sim-mesh/sim/make-zips       # this machine's architecture, in .pio/sim-zips/
```

sim-mesh's pre-built firmware carries aarch64 and x86_64, each built on a
machine of its own architecture: `.github/workflows/build_sim_mesh.yml`
runs `make-zips` on an x86_64 and an arm64 runner and keeps each zip as an
artifact.

The zip's `meshtasticd` is stripped of its symbols and debug sections,
which are some 30 MB of the 32 MB build; `.pio/build/sim-mesh/meshtasticd`
keeps them for debugging.

meshtasticd loads libraries beyond the C library and the C++ runtime
(yaml-cpp, libusb, libi2c and theirs), so `make-zips` puts them in the
zip's `lib/`, from the architecture's multiarch directory. It fills the
zip's `pylib/` with the protobuf bindings generated from this repository's
own `protobufs` and the pure-Python protobuf runtime, fetching
`grpcio-tools` and `protobuf` from PyPI with the pip of the Python running
it. The zip's base names the release, from `version.properties`
(`meshtastic-sx1262-2.7.26_aarch64_<stamp>.zip`), and goes to sim-mesh with
`sim firmware add <zip>`.

**Another architecture on the same machine.** `--arch` builds for another
architecture than the machine's own (more than one `--arch` for several),
in `.pio/build.linux-<arch>/`, with that architecture's cross g++
(`g++-x86-64-linux-gnu` or `g++-aarch64-linux-gnu`) and the native build's
libraries for it from the multiarch packages (`libyaml-cpp-dev:amd64` and
the rest on an arm64 machine). `SIM_MESH_ARCH=<arch>` in pio's environment
selects it: sim-mesh's `radio/portduino/cross.py` swaps in the cross tools,
and its `link.py` compiles the radio with them and links that copy. By hand:

```sh
SIM_MESH_ARCH=x86_64 PLATFORMIO_BUILD_DIR=.pio/build.linux-x86_64 pio run -e sim-mesh
```

The tests run with sim-mesh's Python environment (its `testbed/` gives
`sim_mesh.driver`); `test_host.py` builds the bindings into `sim/.pylib`
the first time:

```sh
cd variants/native/sim-mesh/sim && python3 -m pytest -q
```
