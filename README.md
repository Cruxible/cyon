# 🌀 Cyon
**Cyon** is a hacker-themed desktop environment, tool suite, Minecraft server platform, and scripting engine — all built from scratch in C, Python, Java, and JavaScript. It runs a full X11 window manager, a VTE terminal with a live system visualizer, a pixel art editor, a video editor, a dual-pane file manager, a 3D launcher world, a local AI, a Discord bot, a Flask web dashboard, a Mineflayer combat bot, and a complete suite of custom Paper plugins including **CyonScript** — a scripting language built into Minecraft books.

No cloud. No Electron. No regrets. (Maybe one.)

> ⚠️ Active development. Here be dragons. Pyra knows where they live.

---

## 🗂️ Project Structure

```
~/cyon/
├── src/                  C source — WM, terminal, file manager, pixelart, etc.
├── include/              C headers
├── bin/                  compiled binaries
├── pyra_lib/             Python utility library
│   └── gtk_lib/          GTK-based Python tools
├── cyon_site/            Flask web dashboard
├── cyon_world/           3D pygame launcher world
├── minecraft_stuff/
│   ├── mcbot/            Mineflayer combat bot
│   └── cruxible_plugins/ Custom Paper plugins (Java)
├── assets/
│   ├── gtk.css           Global GTK3 cyan theme
│   └── gtk_red.css       Global GTK3 red theme
├── cyon_config/          Runtime config files
├── compile_cyon          Build script
├── cyon_docs.html        Full HTML documentation
├── README.md             This file (rendered in cyon_about)
└── Modelfile             Ollama Cyon persona
```

---

## 🖥️ cyon_wm — Window Manager

Minimal X11 reparenting WM. Three themes cycle live with no restart via **[ SYSTEM ] → Toggle Theme**.

| Theme | Accent | Compile flag |
|-------|--------|--------------|
| cyon | `#00ffd5` cyan on deep navy | default |
| green | `#00ff99` on near-black | `-DTHEME_GREEN` |
| ember | `#ff8c00` amber on black | `-DTHEME_EMBER` |

### Key Bindings (MOD = Alt)

| Key | Action |
|-----|--------|
| Mod+Enter | Open terminal (cyon_neo) |
| Mod+Space | App launcher |
| Mod+Q | Close focused window |
| Mod+Shift+E | Quit WM |
| Mod+W / Mod+Shift+W | Next / previous wallpaper |
| Mod+1-4 | Switch workspace |
| Mod+Shift+1-4 | Send window to workspace |
| Mod+G | Ghost window (hide from panel, keep running) |
| Mod+drag / Mod+right-drag | Move / resize |
| Volume Up/Down/Mute | Media keys |

### Panels
- **Top** — 4 dropdown menus: `[ ACCESS ]` `[ NODES ]` `[ SYSTEM ]` `[ PROGRAMS ]`. Scrolling marquee. Menus have streaming glitch animation on open.
- **Bottom** — workspace buttons, open window list, clock.

### access.conf — `~/cyon/cyon_config/access.conf`
```
Web Browser    firefox
Steam          steam
---
GIMP           gimp
```

---

## 🖥️ cyon_neo — Terminal + NEO Panel

Full VTE terminal with a live system panel (left) and programmable CMD panel (right).

```
[ ◀ NEO | FULL CAP | REGION CAP | CYON // TTY | spacer | CMD ▶ ]
[ NEO panel ] | [ VTE Terminal ] | [ CMD panel ]
```

- **NEO Panel** — rotating 3D cube (speed/color scales with RAM load), live RAM readout, 12/24h clock, notes area (auto-saves to `cyon_neo.txt`)
- **NEO buttons** — CLEAR · TOP · COPY · PASTE · FONT+ · FONT−
- **Terminal right-click** — Copy · Paste · Clear · htop/top · Font+ · Font−
- **CMD Panel** — up to 8 programmable buttons. Edit with ✎ EDIT CMDS. Config: `cyon_neo_buttons.conf` (`Label|command`)

### ⚠️ .bashrc Guard
```bash
if [ -z "$CYON_NEO" ]; then
    # your interactive menu / program here
fi
```

---

## 🎨 GTK3 Theme System

`~/.config/gtk-3.0/gtk.css` — themes every GTK3 app globally.

| File | Accent | Install option |
|------|--------|----------------|
| `assets/gtk.css` | `#00ffd5` cyan | 15 |
| `assets/gtk_red.css` | `#ff4422` red | 16 |

Edit only the `@define-color` variables at the top to make your own theme.

---

## 🎬 cyon_editor — Video Editor

`pyra_lib/gtk_lib/cyon_editor.py` — GStreamer + ffmpeg video editor with waveform display, trim handles, multi-codec export, audio extraction, frame snapshot, and speed control.

**Keys:** Space · S · ←/→ · I (trim in) · O (trim out) · C (clear) · M · E · P · +/-

---

## 📁 cyon_files — File Manager

Dual-pane GTK file manager registered as system default. Tab · Enter · Backspace · Delete · F2 · Ctrl+C/X/V/N/T/H/R · Right-click.

---

## 🎮 cyon_world — 3D Launcher

`cyon_world/cyon_world_main_v3.py` — first-person pygame world. Walk up to holographic beacons and press E to launch Cyon tools. WASD · mouse look · RMB shoot · Tab free mouse · F11 fullscreen.

---

## ℹ️ cyon_about — About Window

`pyra_lib/gtk_lib/cyon_about.py` — tabbed GTK window from **[ SYSTEM ] → About Cyon**.
- **help tab** — WM keybindings, cyon_files keys, access.conf guide
- **readme tab** — renders `~/cyon/README.md` live with markdown parsing
- `[ docs ]` button opens `cyon_docs.html` in browser

---

## 🐍 pyra_lib Tools

### gtk_lib (GUI)
cyon_editor · cyon_player · cyon_matrix (matrix rain) · cyon_tts · pyra_notes · pyra_downloader · tarmaker_gtk3 · gtk_convert · color_picker · cyon_sc · cyon_about · cyon_pixelart

### CLI (pyra_lib/)
ascii_art · binary_tools · conversions · file_encryptor · find_duplicates · image_converter · figlet_tools · csv2json · convertcsv · mem_scan · moon · net_stress · pyra_repeater · theban · wifi · cybrmsc · tarmaker · shift_image · convert_webp · convert_mvk

---

## 🤖 Ollama + Cyon AI

```bash
curl -fsSL https://ollama.com/install.sh | sh && ollama pull llama3
ollama create cyon -f ~/cyon/Modelfile
```

Managed from `main_cyon` GTK panel. `cyon_local.py` for local chat. `cyon_bot.py` for Discord relay.

---

## 🌐 Web Dashboard

```bash
pip install flask psutil --break-system-packages && python3 cyon_site/app.py
```
`/` (dashboard) · `/games` · `/media` · `/files` — runs at `http://0.0.0.0:5000`

---

## ⚔️ Mineflayer Bot

```bash
npm install mineflayer mineflayer-pathfinder mineflayer-pvp minecraft-data
node minecraft_stuff/mcbot/bot.js
```

`hostile on/off` · `guard on/off` · `protect <n>` · `follow <n>` · `mine on/off` · `drop <player> <item>` · `status` · `!ask <question>` · `!forget`

---

## 🧱 Cruxible Paper Plugins

Build all: `cd minecraft_stuff/cruxible_plugins && ./build_plugins.sh`

---

### 📜 CruxibleBooks — CyonScript

Scripts written in Minecraft books. Save, load, run manually, schedule, or trigger from physical blocks.

**Commands:** `/exportbook` · `/importbook` · `/listbooks` · `/runbook [player]` · `/sendbook` · `/mailbox` · `/linkblock` · `/unlinkblock` · `/listlinks` · `/cyfihere` · `/cyfipoint`

**Language:**
```
say <text>          msg <text>          cmd <command>       opcmd <command>
tp <x> <y> <z>      give <item> [n]     drop <player> <item>
wait <ticks>        repeat <n> <cmd>    cyfi <point> on|off|pulse [ticks]

set x 10            {player} {world} {x} {y} {z} {health} {food}
set half {x} / 2    math: + - * / %

if {x} > 5 ... else ... endif
while {x} > 0 ... endwhile
for i 1 to 10 step 2 ... endfor
# comment
```

**CyFi — Wireless Redstone:** Register named trigger points anywhere on the map. CyonScript fires them remotely. Players have cooldowns. Ops bypass them.

**Scheduled scripts:** Configure in `config.yml` to run at set times daily.

**Block triggers:** Link buttons/plates/levers to scripts with `/linkblock`.

---

### 💰 CruxibleEconomy + Shopkeepers

Sign-based shop economy. Place chest + sign (`[shop]` / item / buy price / sell price). Integrates with Board, Jobs, Ranking, Enchantments.

---

### 🏆 CruxibleRanking

Rank ladder with `/rankup` progression. Configured in `config.yml`. Economy cost to rank up. Integrates with Board sidebar.

---

### ⛏️ CruxibleJobs

Six jobs: Miner ⛏ · Farmer 🌾 · Hunter ⚔ · Lumberjack 🪵 · Fisherman 🎣 · Builder 🧱. XP tracked per job action. Pay multipliers scale with level (1.0x → 1.35x+). Integrates with Economy and Board.

---

### 🗺️ CruxibleLandClaims v2.1

Golden Shovel land claiming. Particle borders (gold=yours, red=other, green=trusted), gold block corner markers, `/claimsee` toggle. Trust levels: Container, Builder, Manager.

---

### ✨ CruxibleEnchantments

Custom enchantments bought with Economy: Tunnel Vision · Auto Smelt · Vein Breaker (I-III) · Green Thumb · Night Vision · Knockback X (I-X) · Water Breathing · Speed Boost (I-III) · Life Steal (I-III).

---

### ⚡ CruxiblePowers

Drop named item to activate special abilities with cooldowns and particle effects.

---

### Other Plugins

| Plugin | Function |
|--------|----------|
| CruxibleHomes | Named home locations (`/sethome`, `/home`, `/homes`) |
| CruxibleKits | Configurable item kits with cooldowns |
| CruxibleRepair | `/repair` and `/repair all` with Economy cost |
| CruxibleLifesteal | Drop hearts on death; pick up to steal max HP |
| CruxibleTPA | `/tpa`, `/tpaccept`, `/tpdeny`, `/tpahere` |
| CruxibleBoard | Live sidebar: balance, rank, job, HP, coords |
| CruxibleChatLink v2 | Minecraft ↔ Discord chat bridge |
| CruxibleMap | Custom in-game map viewer command |
| CruxibleHelp | Configurable `/help` replacement |

---

## 📦 Dependencies

```bash
# C / GTK
sudo apt install -y build-essential libgtk-3-dev libvte-2.91-dev \
    pkg-config curl yt-dlp ffmpeg scrot xclip libx11-dev libpng-dev

# Python
sudo apt install -y python3-gi python3-gi-cairo gir1.2-gtk-3.0 python3-venv nodejs npm
```

---

## 🛠️ Building

```bash
chmod +x compile_cyon && ./compile_cyon
```

| # | Description |
|---|-------------|
| 1 | Install all deps + compile everything |
| 2 | cyon_wm + cyon_sc + cyon_launch + cyon_files |
| 3 | cyon desktop apps |
| 4-7 | cyon_shell / notes / neo / pixelart |
| 8 | pyra_env + Piper TTS |
| 9-10 | pyra_toolz / pyra_termux |
| 11-12 | PATH / Piper voices |
| 13-14 | Remove cyon_shell / PATH |
| **15** | **Install GTK theme — Cyan** |
| **16** | **Install GTK theme — Red** |
| 17 | Uninstall Cyon |

---

## 🗑️ Uninstall

Option **17** from `compile_cyon`. Removes everything. Asks once. Means it.
