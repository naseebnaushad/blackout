# Blackout

![Blackout](screenshot.png)

**Blackout** is an OLED screen curtain, `zwlr_layer_shell_v1` `LayerOverlay` key-combo lock, and OLED care utility for Omarchy. When you leave long-running tasks running on your laptop, Blackout locks a dedicated Wayland `LayerOverlay` surface over the entire display (above the status bar and notifications so Hyprland can never tile or shrink it), drops hardware panel brightness to `0%`, silences notification toasts, and inhibits system idle/sleep until you press your unlock key combination.

## Features

- **Wayland `LayerOverlay` Curtain (`layer-shell-qt`)**: Runs on `LayerShellQt::Window::LayerOverlay` with `exclusiveZone = -1` and `KeyboardInteractivityExclusive`—immune to workspace tiling, window activations, or top-bar reservations.
- **0% Hardware Backlight Cut**: Saves your current panel brightness (`brightnessctl` / `/sys/class/backlight`), sets panel drive to `0%` while locked, briefly flashes at `5%` for 1.6s if a wrong key is pressed so you can see the unlock hint, and restores your exact brightness on unlock.
- **Automatic Do-Not-Disturb**: Silences Omarchy notification toasts (`omarchy-shell notifications setDnd on`) while locked so incoming notifications never light up the OLED panel, and restores your previous DND state on unlock.
- **Key-Combo Lock**: Swallows single keys, Escape, Space, Q, and mouse clicks while active—exits only when your exact unlock shortcut (default `Ctrl+Shift+U`) is pressed.
- **OLED Luminance Segmented Presets & Warm Night Light**: One-key presets (`20%`–`100%`) to cap OLED brightness during work sessions and a live toggle for 4000K Night Light to reduce blue OLED sub-pixel aging.
- **OLED Pixel Refresher**: Built-in 60-second rolling wash bar + RGB/CMY sub-pixel leveling sweep (`P`) to clear temporary image retention.
- **Headless CLI & Agent Skill**: Query status, check dependencies, or configure settings without a display via `blackout status --json`, `blackout check --json`, `blackout set ... --json`, and `blackout skill install`.

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `Space` | Start OLED blackout |
| `Ctrl+Shift+U` | Default unlock key combo (configurable) |
| `R` | Record a new unlock key combo |
| `Backspace` | Reset unlock key combo to `Ctrl+Shift+U` |
| `B` | Toggle 0% hardware backlight cut during blackout |
| `D` | Toggle notification silencing (DND) during blackout |
| `A` | Toggle auto-blackout inactivity timer |
| `-` / `+` | Decrease / increase inactivity timeout minutes |
| `I` | Toggle pixel-shifted status badge |
| `1` – `5` | Set OLED brightness preset (`20%`, `40%`, `60%`, `80%`, `100%`) |
| `N` | Toggle warm Night Light (`4000K`) |
| `P` | Run 60-second OLED Pixel Refresher |
| `Ctrl+Z` / `Ctrl+Shift+Z` | Undo / Redo setting change |
| `?` | Show / hide keyboard shortcuts |
| `Q` | Quit (disabled while blackout is locked) |

## CLI Commands

```sh
blackout open                                  # Open the GUI window
blackout lock --quit-on-unlock                 # Immediately enter locked OLED blackout
blackout status --json                         # Print current settings and hardware state
blackout check --json                          # Check required tools and backlight sysfs
blackout set --unlock Ctrl+Alt+U --auto on     # Update settings headlessly
blackout skill install                         # Install the agent skill
```

## Build & Install

```sh
./bin/build
./bin/test
./bin/install
```
