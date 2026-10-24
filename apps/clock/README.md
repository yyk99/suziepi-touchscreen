# clock

Digital clock for the SuziePi 3.5" screen (LVGL 9, fbdev + evdev). Shows
`HH:MM` and the date in the Pi's local time zone, with an **Exit** button.
Built with **meson**, cross-compiled on a development host and copied to the Pi.
The scripts in `scripts/` are meant to be run **on the Pi** (no ssh).

## Host requirements (build machine)

```sh
sudo apt install meson ninja-build gcc-arm-linux-gnueabihf
```

(`pip install meson` also works.) The first configure downloads LVGL v9.2.2
into `subprojects/lvgl/` (needs network and `git`).

## Build (on the build machine)

```sh
cd apps/clock
meson setup build-arm --cross-file cross/pi3.ini
meson compile -C build-arm            # -> build-arm/clock (static ARM binary)
```

Get `build-arm/clock` and this `apps/clock` directory (or a git checkout of the
repo) onto the Pi by any means you like (git, scp, USB stick).

## Install and run (on the Pi)

```sh
cd apps/clock
scripts/install.sh path/to/clock       # copies to ~/bin/clock, restarts the service if running
~/bin/clock                            # run in the foreground
~/bin/clock -f /dev/fb1 -t /dev/input/event0   # explicit devices
```

`install.sh [binary] [dest-dir]` defaults to `./clock` and `$HOME/bin`. The `pi`
user is in the `video` and `input` groups, so no sudo is needed (otherwise run
with sudo). Stop with the on-screen Exit button or Ctrl-C. The harmless message
`ioctl(FBIOBLANK): Invalid argument` comes from fbtft not supporting blanking.

## Start at boot (systemd)

`systemd/clock.service` runs `/home/pi/bin/clock` as user `pi` (groups `video`
and `input`, so no root needed) at boot. It restarts after a crash but not
after the on-screen Exit button. Install the binary first (see above), then use
the helper script on the Pi:

```sh
scripts/service.sh install     # copy the unit to /etc/systemd/system (does not enable it)
scripts/service.sh enable      # start now and at every boot
scripts/service.sh disable     # stop now and do not start at boot
scripts/service.sh status      # show service state
scripts/service.sh logs        # last 50 journal lines
scripts/service.sh uninstall   # disable and remove the unit
```

`start`, `stop` and `restart` also work. The script uses `sudo` for the
systemd commands.

The same thing by hand:

```sh
sudo cp systemd/clock.service /etc/systemd/system/ && sudo systemctl daemon-reload
sudo systemctl enable --now clock.service    # enable at boot, start now
sudo systemctl disable --now clock.service   # disable at boot, stop now
```

`scripts/install.sh` restarts a running service itself. While the
service runs, a manual `clock` run would compete for the screen, so stop the
service first (`scripts/service.sh stop`). The console on HDMI is unaffected.

## Native build (for a quick compile check)

```sh
meson setup build-native && meson compile -C build-native
```

## Design notes

- **Static linking:** `cross/pi3.ini` links with `-static`. The host glibc
  (2.39) is newer than the Pi's (2.36); a dynamic binary can reference symbols
  the Pi lacks. Static binaries avoid that and need no sysroot.
- **CPU flags:** `-mcpu=cortex-a53 -mfpu=neon-vfpv4 -mfloat-abi=hard` targets
  the Pi 3 family running 32-bit Raspberry Pi OS. It will not run on a Pi
  Zero/1 (ARMv6). Remove those flags for a generic ARMv7 build.
- **LVGL under meson:** upstream has no `meson.build`, so
  `subprojects/packagefiles/lvgl/` provides one (applied by
  `subprojects/lvgl.wrap`). It compiles all `src/**/*.c` as C only, so no C++
  compiler is needed. LVGL options are global compile arguments in the top
  `meson.build` (no `lv_conf.h`).
- **Time:** the Pi syncs time over NTP and uses `/etc/localtime`. The clock
  reads `localtime_r`, so change the zone on the Pi with `timedatectl`.
- Touch calibration constants and device discovery are copied from
  `../lvgl-hello`; factor them into a shared library if more apps are added.
