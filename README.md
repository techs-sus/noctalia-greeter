# Noctalia Greeter

**_quiet by design_**


<p align="center">
  <img src="https://assets.noctalia.dev/noctalia-logo.svg?v=2" alt="Noctalia Logo" style="width: 192px" />
</p>
 
 
 
 

---

## What is Noctalia Greeter?

A minimal, modern login greeter for [greetd](https://github.com/kennylevinsen/greetd), designed to match the Noctalia look and feel.

It runs as a Wayland client inside [Cage](https://github.com/cage-kiosk/cage), uses the same UI/theming stack as [Noctalia Shell](https://github.com/noctalia-dev/noctalia-shell), and focuses on a clean, reliable authentication flow.

---

## 📋 Requirements

- `greetd`
- `cage`
- `dbus` (`dbus-run-session`)
- A `greeter` system user

Build tools:

- Meson + Ninja
- C++20 compiler
- `pkg-config`
- `just` (optional, but recommended)

Build-time libraries (pkg-config names):

- `wayland-client`, `wayland-protocols`
- `xkbcommon`
- `freetype2`, `fontconfig`
- `cairo`, `cairo-ft`, `pango`, `pangocairo`, `pangoft2`
- `librsvg-2.0`
- `glib-2.0`, `gobject-2.0`, `gio-2.0`
- `egl` / `glesv2` / `wayland-egl` (or `epoxy` fallback)
- `libwebp`

---

## 🚀 Getting Started

### 1) Build

```bash
meson setup build
meson compile -C build
```

or:

```bash
just build
```

### 2) Install

```bash
just install
```

This installs the greeter binaries, session launcher, polkit policy, and assets, then runs system setup:

- `scripts/setup_greetd_pam.sh`
- `scripts/setup_greeter_system.sh`

### 3) Configure greetd

Add this to `/etc/greetd/config.toml`:

```toml
[default_session]
command = "/usr/local/bin/noctalia-greeter-session"
user = "greeter"
```

Optional default session (tuigreet-style `--cmd`); must match a Wayland session **Name** from the picker:

```toml
command = "/usr/local/bin/noctalia-greeter-session -- --session niri"
```

If your install prefix is different, use the installed path for `noctalia-greeter-session`.

### 4) Restart greetd

```bash
sudo systemctl restart greetd
# or
sudo sv restart greetd
```

---

## Packaging

Meson installs the following (paths use your `prefix`, commonly `/usr/local`):

| Artifact | Install location | Role |
|----------|------------------|------|
| `noctalia-greeter` | `bindir` | Login UI (Wayland client under Cage) |
| `noctalia-greeter-session` | `bindir` | greetd session command (`cage` + greeter) |
| `noctalia-greeter-apply-appearance` | `bindir` | Root helper for shell -> greeter appearance sync |
| `assets/` | `share/noctalia-greeter/assets` | Fonts, icons, etc. |
| `org.noctalia.greeter.apply-appearance.policy` | `share/polkit-1/actions` | polkit rule for the sync helper |

**Runtime paths**:

- `/var/lib/noctalia-greeter/`: synced appearance (`appearance.json`, `wallpaper.*`) and `greeter.conf` (session/scheme prefs; see below)
- `/var/log/noctalia-greeter.log`, `/var/lib/noctalia-greeter/greeter.log`: logs (`just setup-log-dir`)

**Environment overrides** (optional):

- `NOCTALIA_GREETER_STATE_DIR`: synced appearance directory (default `/var/lib/noctalia-greeter`)
- `GREETER_USER`: greeter account for setup/logs only
- `NOCTALIA_GREETER_ASSETS_DIR`: asset root override

### Appearance sync (Noctalia Shell v5)

Appearance sync is only supported with **[Noctalia Shell v5](https://github.com/noctalia-dev/noctalia-shell/tree/v5)** (the `v5` branch). Older shell releases do not include the settings control or staging flow.

From **Settings -> Shell -> Security -> Noctalia Greeter -> Sync Now**, the shell:

1. Stages `appearance.json` (and a wallpaper file when needed) under the user’s `$XDG_RUNTIME_DIR/noctalia-greeter-sync/`
2. Runs `pkexec noctalia-greeter-apply-appearance <staging-dir>` (admin prompt via polkit)
3. Installs into `/var/lib/noctalia-greeter/` as root-owned, world-readable files (`0755` dir, `0644` data). No greetd user lookup.

The greeter reads `appearance.json` on startup and adds a **Synced** entry to the color-scheme picker (built-in palettes keep solid backgrounds). When synced data is present, **Synced** is the default scheme. Session and scheme preferences live in `/var/lib/noctalia-greeter/greeter.conf` (see below). **Both packages must be installed** (shell v5 + greeter + polkit policy). After syncing, restart greetd or log out once to see the shell wallpaper and palette.

### `greeter.conf`

Simple `key = value` file (`#` comments). Values must match a discovered session **Name** (`.desktop` `Name=`, same as the picker label) and a listed scheme name.

| Key | Set by | Purpose |
|-----|--------|---------|
| `greeter_user` | install setup | greetd account (logs); not used at runtime for sync |
| `default_session` | you (config or `--session` CLI) | default session on the picker when the greeter starts |
| `session` | greeter UI | last session used (picker or last successful login) |
| `scheme` | greeter UI | last color scheme picked |

**Initial session** on startup: `--session` / `--cmd` → `default_session` → `session` (last used) → first discovered session.

Example:

```ini
greeter_user=greetd
default_session="niri"
session="Hyprland"
scheme="Synced"
```

Opens with **niri** selected. If the user picks Hyprland, only `session` is updated; `default_session` stays **niri** for the next login. Without `default_session` or `--session`, the greeter uses `session` (last used).

If last-used never seems to stick, check that `default_session` is not set (it always wins over `session`), that `session=` in `greeter.conf` updates after login (`noctalia-greeter sessions` for exact **Name** spelling), and greeter logs for `failed to save greeter.conf`.

List available session names (for `default_session` / `--session`):

```bash
noctalia-greeter sessions
```

`just install` runs `setup_greeter_system.sh`, which calls `noctalia-greeter-apply-appearance --setup-system` to create `greeter.conf` and give the greetd user write access so UI changes persist. Appearance sync does not depend on `greeter_user`.

Manual test of the helper (as root), after staging a directory:

```bash
sudo ./build/noctalia-greeter-apply-appearance /run/user/1000/noctalia-greeter-sync
```

---

## Keyboard

The greeter is fully operable without a pointer.

| Key | Action |
|-----|--------|
| `Tab` / `Shift+Tab` | Move focus forward / backward through the focus ring (works from inside the password field too) |
| `↑` / `↓` | Move focus, or move the highlight when a dropdown menu is open |
| `Enter` | Submit the password / activate the focused control / confirm the menu highlight |
| `Space` | Activate the focused control (non-text controls) |
| `Esc` | Close an open menu, or leave the password step |
| `F3` | Open / close the **session** picker from anywhere |
| `F7` | Open / close the **color scheme** picker from anywhere |

---

## Troubleshooting

- **Blank screen / greeter does not start.** Check the logs at `/var/log/noctalia-greeter.log` and `/var/lib/noctalia-greeter/greeter.log` (run `just setup-log-dir` once if they are missing). The greeter logs its version and the build id at startup.
- **`WAYLAND_DISPLAY is not set`.** The greeter must run under a Wayland compositor; greetd launches it via `noctalia-greeter-session` (Cage). Verify your greetd `command` points at that script.
- **Session or scheme preference ignored.** Values in `greeter.conf` must match a discovered session **Name** (`noctalia-greeter sessions`) and a listed scheme. Unrecognized keys and malformed lines are now skipped with a warning in the log (with the offending line number) instead of failing silently.
- **Appearance sync not applied.** Confirm both Noctalia Shell v5 and the polkit policy are installed, then restart greetd or log out once. See [Appearance sync](#appearance-sync-noctalia-shell-v5).

---

## Development

Run locally under Cage:

```bash
just run-local
```

Run inside an existing Wayland session:

```bash
just run-niri
```

To force single-monitor mode for debugging, use Cage with `-m last`:

```bash
dbus-run-session cage -s -m last -- ./build/noctalia-greeter
```

AddressSanitizer:

```bash
just run-cage-asan
```

Recovery helper:

```bash
just recover
```

---

## Scope

Noctalia Greeter is a **display/login greeter** for greetd. It handles user/session selection and authentication UI.

It is **not** a desktop shell or compositor replacement.

---

## 🤝 Contributing

Contributions are welcome: fixes, polish, docs, or UX improvements.

- Open an issue for bugs and regressions
- Open a PR for improvements

---

## 📄 License

MIT License.

