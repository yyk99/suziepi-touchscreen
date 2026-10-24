# komaro-digest

Shows the latest sensor values from the KOMARO InfluxDB database on the SuziePi
3.5" screen. Built on the shared `suzie_platform` library. See `~/src/komaro`
for the pipelines that fill the database.

## Pages

Swipe sideways, or use the tabs at the bottom of the screen.

```
┌─────────┬────────────────────────────────────────────────────────────────────────────────────────┐
│ Page    │ What it shows                                                                          │
├─────────┼────────────────────────────────────────────────────────────────────────────────────────┤
│ Current │ Sensor name, temperature in C and F, humidity, and when the value was measured. The    │
│         │ status line turns orange when the value is older than 15 minutes, and red when the     │
│         │ database cannot be reached (the last values stay on screen).                           │
├─────────┼────────────────────────────────────────────────────────────────────────────────────────┤
│ Sensors │ The InfluxDB measurements (living_room, sensor, sensor2, ...). Tap one to show it; the │
│         │ app returns to the Current page and remembers the choice. The list is re-read each     │
│         │ time the page is opened.                                                               │
└─────────┴────────────────────────────────────────────────────────────────────────────────────────┘
```

A measurement without a `temperature_c` field is shown as a plain list of its
first fields. The red Exit button in the corner ends the program.

## Build and run

Built with the rest of `apps/` (see `../README.md`). The binary is
`build-arm/komaro-digest/komaro-digest`. Copy it to the Pi and run it there;
stop the clock service first, because only one app can use the screen:

```sh
apps/clock/scripts/service.sh stop
./komaro-digest
./komaro-digest -H 192.168.68.53 -s sensor2      # options override the config
```

### Options

```
┌─────────┬───────────────────────────────────────────────┬──────────────────────────┐
│ Option  │ Meaning                                       │ Default                  │
├─────────┼───────────────────────────────────────────────┼──────────────────────────┤
│ -H host │ InfluxDB host (name or IPv4 address)          │ silvana.home             │
│ -P port │ InfluxDB HTTP port                            │ 8086                     │
│ -d name │ Database                                      │ komaro                   │
│ -s name │ Sensor (measurement) to show first            │ last choice, else sensor │
│ -u C|F  │ Unit of the large temperature                 │ last choice, else C      │
│ -r secs │ Seconds between re-reads of the data (min 5)  │ 180                      │
│ -f, -t  │ Framebuffer and touch device (suzie_platform) │ fb1, ADS7846             │
│ -h      │ Print this list of options and exit           │                          │
└─────────┴───────────────────────────────────────────────┴──────────────────────────┘
```

## Start at boot (systemd)

`systemd/komaro-digest.service` runs `/home/pi/bin/komaro-digest` as user `pi`
(groups `video` and `input`, no root needed) once the network is up. It
restarts after a crash but not after the on-screen Exit button. Run these on the
Pi (no ssh), from this directory:

```sh
scripts/install.sh path/to/komaro-digest   # copy to ~/bin, restart service
scripts/service.sh install     # copy the unit to /etc/systemd/system
                               # (does not enable it)
scripts/service.sh enable      # start now and at every boot
scripts/service.sh disable     # stop now and do not start at boot
scripts/service.sh status      # show service state
scripts/service.sh logs        # last 50 journal lines
scripts/service.sh uninstall   # disable and remove the unit
```

The screen can show only one app. The unit has `Conflicts=clock.service`, and
`enable` disables the clock service so that only one of them starts at boot.
`../clock/scripts/service.sh enable` switches back. Options for the service
(host, refresh interval) go in `~/.config/komaro-digest/config`.

## Settings files

Both live in `~/.config/komaro-digest/` (or `$XDG_CONFIG_HOME/komaro-digest/`)
and both are optional.

```
┌──────────┬────────────────────────────────────────────────────────────────────────┐
│ File     │ Content                                                                │
├──────────┼────────────────────────────────────────────────────────────────────────┤
│ config   │ Written by hand: host=, port=, db=, refresh= lines (# = comment)       │
│ state    │ sensor=... and unit=C or F, as set on the Sensors page. Written by app │
└──────────┴────────────────────────────────────────────────────────────────────────┘
```

Order of precedence: command line, then `config` / `state`, then the defaults
above.

## How it works

- Data: a plain HTTP GET on the InfluxDB 1.x `/query` endpoint. `SHOW
  MEASUREMENTS` gives the sensor list, and `SELECT * FROM "<name>" ORDER BY time
  DESC LIMIT 1` gives the latest values. Every field in the reply that is a
  number is kept.
- Worker thread: `src/model.c` fetches in the background, every 180 s (`-r`) and on
  demand (sensor switched, Sensors page opened). LVGL is only touched from the
  main thread, which polls a snapshot twice a second, so an unreachable server
  never freezes the screen (requests time out after 5 s).
- No libraries: `src/json.c` is a small JSON parser, `src/influx.c` the client
  and `src/net.c` the sockets code.
- Own name resolution: the binary is linked statically (see `../README.md`), and
  a static glibc cannot load the NSS modules at run time; calling
  `getaddrinfo()` crashed on the Pi. `net_resolve_ipv4()` therefore reads an
  IPv4 literal, then `/etc/hosts`, then asks the nameservers from
  `/etc/resolv.conf` with a DNS A query. mDNS (`.local`) names are not resolved;
  use a DNS name or an address.
- No authentication or HTTPS: this matches the default InfluxDB 1.x setup used
  by the other KOMARO tools.

## Tests

The JSON parser, the InfluxDB reply parsers and the resolver have unit tests in
`tests/test_parse.c`. They run in a native build only:

```sh
cd apps
meson setup build-native && meson test -C build-native
```
