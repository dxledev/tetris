# Tetris

A portable, standalone falling-block game for Linux, written in C++20 and Qt Widgets. A clean, resizable window, solid colors, and a theme CLI that updates the game while you play.

- Endless: keep playing as the speed increases.
- Classic: clear 40 lines as quickly as you can.
- Practice: place pieces at your own pace, with automatic falling disabled.
- Seven-piece bags, wall kicks, hold, five upcoming pieces, and a landing guide.
- Separate top-ten Endless and Classic leaderboards, plus adjustable movement, drop, and lock timing.
- Native Wayland and X11 support. Raster rendering needs no dedicated GPU.

Using a prebuilt archive? Extract it and run `./tetris`. Compatible Qt 6 and C++ runtime libraries must be installed. The included installer can add the app to your launcher.

## Build and run

Requires a C++20 compiler, CMake 3.21+, and Qt 6.4+ with Core, Gui, and Widgets. Tests also require Qt Test. Ninja is optional.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
./build/tetris
```

Choose a mode at launch with `--mode endless`, `--mode classic`, or `--mode practice`. `--seed 42` gives a repeatable piece sequence. The window remembers its size and your settings.

For a local installation:

```bash
bash scripts/install-desktop.sh --no-follow --no-apps-list --dry-run
bash scripts/install-desktop.sh --no-follow --no-apps-list
```

The installer puts `tetris` in `~/.local/bin` and registers a Tetris application shortcut and icon. Add `~/.local/bin` to your `PATH` if needed. Existing files are backed up before replacement. Installation paths can be set with `--prefix`, `--config-dir`, and `--build-dir`.

For a standard system package, use `cmake --install build --prefix /your/prefix`. Package maintainers can set `DESTDIR` for staging.

## Theme from the CLI

```bash
tetris theme list
tetris theme preset paper
tetris theme set 'background=#101819' 'accent=#b6e3c6' 'piece_t=#c69bee'
tetris theme import themes/forest.json
tetris theme export /tmp/my-tetris-theme.json
```

Every change is validated, saved atomically to `theme.json`, and picked up by open windows. Your last colors remain available after closing the app. Theme commands work without a running game or a display server.

Add `--dry-run` to preview a change without writing anything:

```bash
tetris theme preset rose --dry-run
tetris theme set 'accent=#ffcc66' --dry-run
```

To follow a palette managed by another tool:

```bash
tetris theme follow file /absolute/path/to/palette.json
tetris theme follow static /absolute/path/to/theme-directory
tetris theme detach
```

Following reloads colors when the file changes, is replaced, or a directory symlink switches targets. A theme directory can contain `theme.json`, `Colors.qml`, `palette.json`, `colors.json`, or `kitty.conf`. Invalid updates keep the last accepted colors. `detach` freezes the current palette. A preset, import, or manual color edit also stops following a source.

Find or inspect the saved palette:

```bash
tetris theme path
tetris theme show
tetris theme keys
```

The default location is `${XDG_CONFIG_HOME:-~/.config}/tetris`. Override it with `--config-dir DIRECTORY` or `TETRIS_CONFIG_DIR` for both the app and the CLI.

Exported themes are JSON objects with `version: 1`, a `name`, and a complete `colors` object. Use six-digit `#RRGGBB` colors. The included palettes show the full format.

| Colors | Used for |
| --- | --- |
| `background`, `surface`, `surface_alt`, `board` | Window, cards, controls, playfield |
| `text`, `muted`, `accent`, `danger` | Text, actions, status |
| `border`, `grid`, `ghost`, `selection` | Outlines, guides, selected controls |
| `piece_i`, `piece_o`, `piece_t`, `piece_s`, `piece_z`, `piece_j`, `piece_l` | The seven pieces, hold, and previews |

You can edit `theme.json` directly. Keep it valid JSON; the app reloads a complete, valid update without resetting the game.

## Controls

| Key | Action |
| --- | --- |
| Left / Right | Move |
| Down | Soft drop |
| Up / X | Rotate clockwise |
| Z | Rotate counterclockwise |
| Space | Hard drop |
| C / Shift | Hold or swap a piece |
| Enter | Start, resume, or play again |
| P / Escape | Pause |
| Ctrl+N | New game |
| F1 | Show controls |

Use Settings to adjust keyboard repetition, the time before a piece locks, the landing guide, the grid, and automatic pause when the window loses focus. Settings and records stay on your device.

Stats shows your ten highest scores in Endless and Classic, with each mode’s best score highlighted. Scores are saved locally in `records.json` when a game ends, is restarted, changes mode, or is closed. Existing personal bests carry over; Practice scores are excluded.

Stats, settings, and controls open inside the game window over a blurred backdrop. Play pauses while they are open; Escape or a click outside the card dismisses them.

For finer control, edit `settings.json` while the app is closed. It also exposes `gravity_initial_ms`, `gravity_step_ms`, `minimum_gravity_ms`, `lines_per_level`, and `max_lock_resets`.

## Verify and package

```bash
ctest --test-dir build --output-on-failure
bash scripts/package.sh --dry-run
bash scripts/package.sh
```

The package includes the executable, themes, installer, icon, and documentation. Extract it and run `./tetris`, or use the included installer. Prebuilt Linux binaries require compatible Qt 6 and C++ runtime libraries; build from source for other systems. The game uses no browser runtime or background service.

Licensed under MIT.
