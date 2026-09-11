# xfce4-mediaplayer-plugin

A native C/GTK3 panel plugin for the Xfce panel that controls music
playback over **MPRIS/D-Bus**. It works with any player that exposes the
`org.mpris.MediaPlayer2` interface — Spotify, VLC, Rhythmbox, mpv,
Firefox, Chrome, and others — with no per-application integration
required.

> **⚠ Early development.** This plugin is under active development,
> has only been tested on a single machine/configuration (Ubuntu
> 26.04, Xfce 4.20), and hasn't had a stable release yet. Expect
> rough edges, and treat the `.deb` and instructions below as a
> moving target rather than a finished product.

## Features

- Previous / play-pause / next buttons, wired to the active MPRIS player.
- Track info label with independently toggleable **title**, **artist**
  and **album** fields.
- Album art, fetched from the player's `mpris:artUrl` and shown to the
  left of everything else, only when the panel row is tall enough to
  display it well.
- Progress bar rendered as a background wash behind the track text
  (rather than a separate element), with an elapsed/total time
  indicator (`m:ss / m:ss`) in a fixed-width column to the right of the
  text.
- The bar's colors adapt to the current Xfce/GTK theme: the theme's
  own accent color is used for the filled portion (so it stands out
  against the panel background in any theme), and the text gets a
  subtle shadow in the opposite shade of its own color so it stays
  legible regardless of what's directly behind it. Recalculated live
  if the theme changes while the panel is running.
- Configurable label width (in characters) and configurable position
  for the playback controls (left or right of the text).
- Preferred player selection (or automatic: whichever MPRIS player is
  currently playing), from a dropdown of the players currently on the
  session bus.
- Brief tolerance for metadata gaps: if a player momentarily reports no
  track info (e.g. the instant between two tracks), the previous
  track's text is kept on screen for a few seconds instead of
  immediately flashing "No player".
- Live updates via MPRIS `PropertiesChanged` D-Bus signals — no
  polling, except for playback position, which MPRIS does not signal
  and which is queried once a second only while something is actually
  playing and the progress indicator is visible.

## Requirements

Runtime: a desktop session with a D-Bus session bus and at least one
MPRIS-compatible media player. Works under both X11 and Wayland Xfce
sessions — the plugin has no X11-specific code; it only uses MPRIS
(D-Bus), GTK3, and the standard `libxfce4panel` plugin API, all of
which are display-server agnostic.

Build dependencies are listed under each build option below — installing
from a `.deb` (Option A) does *not* require them on the machine doing
the install, only on the one building the package.

## Building and installing

Two ways to get the plugin onto a machine: build a `.deb` once and
install that (no compiler or `-dev` packages needed on the machine
that installs it), or build straight from source there.

### Option A: build a `.deb` package

This is the way to go if you want to install the plugin on a machine
without setting up the whole build toolchain there, or just want
`apt`/`dpkg` to track it as a normal package (clean removal, no stray
files).

**A `.deb` is tied to the system it was built on.** A package built on
**Ubuntu 26.04 (amd64) with Xfce 4.20** installs directly (`apt
install ./the.deb`) on any other machine running that *same*
combination — same Ubuntu release, same architecture. Anything else
(a different distro, a notably older/newer Ubuntu or Xfce version, a
different CPU architecture) is very likely to have `apt` refuse the
package outright over unresolvable dependencies (exact package names
like `libgtk-3-0t64` only exist on Ubuntu 24.04+), or in rarer cases
install but not fully work — Xfce's panel plugin ABI has changed
enough between versions that this project already had to work around
version-specific quirks in Xfce 4.20 itself. For any target that
doesn't match, use **Option B** and build on that machine instead.

Build dependencies (only needed on the machine *building* the
package):

```bash
sudo apt update && sudo apt install -y \
  build-essential debhelper pkgconf libglib2.0-dev libgtk-3-dev \
  libxfce4panel-2.0-dev libxfce4ui-2-dev xfce4-dev-tools
```

Build it:

```bash
dpkg-buildpackage -us -uc -b
```

This produces `../xfce4-mediaplayer-plugin_<version>_<arch>.deb`.
Copy that file to wherever you want to install it — it's a normal
Debian package, `apt`/`dpkg` resolve its runtime dependencies (GTK3,
`libxfce4panel`, `xfce4-panel`, …) automatically:

```bash
sudo apt install ./xfce4-mediaplayer-plugin_0.2.0-1_amd64.deb
```

If the file lives under your home directory, `apt`'s `_apt` sandbox
user may not be able to read it (a `700`-permission home directory is
common) — copy it to `/tmp` first and install from there if `apt`
reports a permission error acquiring the file.

### Option B: build and install from source directly

```bash
sudo apt update && sudo apt install -y \
  build-essential pkgconf libglib2.0-dev libgtk-3-dev \
  libxfce4panel-2.0-dev libxfce4ui-2-dev xfce4-dev-tools
./autogen.sh
./configure --prefix=/usr
make
sudo make install
```

Either way, `sudo`/root is unavoidable at install time: the panel only
looks for plugin modules in its system plugin directory (e.g.
`/usr/lib/<triplet>/xfce4/panel/plugins/`), never in any per-user
directory, so a plain `make install` into `$HOME` — or a build with no
elevated install step at all — will never be picked up.

### If you're actively editing the source

`make` alone is only safe for incremental rebuilds if header-dependency
tracking is enabled — and it silently isn't, in a tree that's ever been
configured by **Option A** (`dpkg-buildpackage` runs `./configure` via
debhelper, which defaults to `--disable-dependency-tracking`). In that
state, `make` doesn't know `mediaplayer-dialogs.c` and
`mediaplayer-mpris.c` depend on `mediaplayer.h`, so editing that header
and running plain `make` rebuilds only `mediaplayer.c` — linking a
`.so` from object files compiled against *different* versions of the
`MediaplayerPlugin` struct. That's silent memory corruption, not a
compile error, and it's surfaced as GTK crashing in unrelated places
(e.g. inside `gtk_widget_show()`) rather than as anything pointing back
at the real cause.

If you only ever build via Option B in a fresh checkout, dependency
tracking is on by default and this doesn't apply. Otherwise, either
run `make clean` before `make` whenever a `.h` file changed, or fix it
at the source by reconfiguring without debhelper's flags:

```bash
./configure --prefix=/usr
```

### After installing (either option)

Restart the panel:

```bash
xfce4-panel -r
```

Then right-click the panel → **Panel** → **Add New Items…** and add
"Media Player".

## Configuring

Right-click the plugin on the panel → **Properties** to open the
preferences dialog:

- **Preferred player** — pick a specific player, or leave it on
  "Auto" to follow whichever MPRIS player is currently playing.
- **Show title / Show artist / Show album** — toggle each field of
  the track info label independently.
- **Show album art** — shown only when the panel row is tall enough.
- **Show progress bar** — the elapsed/total time indicator and the
  background progress bar.
- **Label width (characters)** — how wide the track text (and the bar
  behind it) is allowed to grow before it gets ellipsized.
- **Controls position** — put the previous/play-pause/next buttons to
  the left or right of the text. Album art always stays on the far
  left regardless of this setting.

## Known limitations

- Album art is only loaded for `file://` URLs (what most desktop
  MPRIS players expose, usually pointing at a locally cached copy of
  the artwork). Players that only provide a remote `http(s://)` art
  URL won't show a thumbnail.
- Playback position isn't part of the MPRIS change-notification
  signal, so it's polled once a second while a track is playing; this
  is a deliberate, low-frequency D-Bus call, not continuous polling.
- Vertical panels (`Vertical` mode, a rotated column of stacked
  plugins) aren't supported — the plugin's row-based layout has no
  vertical equivalent, so it shows a small warning icon instead of a
  squashed/broken UI. Deskbar mode (a vertical panel with
  horizontally-laid-out plugins, e.g. most docks) works normally.
