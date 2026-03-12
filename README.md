# 🌀 Cyon
Cyon is a hybrid GTK and CLI-based desktop tool built in C. It manages local AI (Ollama + Llama 3), a Discord bot, a media downloader, and a launcher for a suite of Python utility tools. Lightweight, hacker-themed, and extensible.

No cloud. No Electron. No regrets. (Maybe one.)

> ⚠️ Currently under active development. Here be dragons. pyra knows where they live.

---

## ✨ Features

- ✅ GTK 3 control panel — manage Ollama, local AI, and Discord bot from one window
- ✅ Local AI chatbot powered by Ollama + Llama 3 (no cloud, no API key, no snitching)
- ✅ Discord bot — relay messages through local Llama 3 like a hacker puppet master
- ✅ MP3/MP4 and file downloading via yt-dlp and curl (yes, legally. probably.)
- ✅ Standalone CLI with styled shell prompt — for when the GUI offends you
- ✅ Shell command processor — slash-commands wired directly to the GTK input field
- ✅ Modular design — built to be extended, broken, and extended again
- ✅ Linux-focused — other OSes: not our problem
- ✅ Pyra tool launcher — launches pyra_toolz directly from the Programs menu
- ✅ pyra_lib — a collection of Python utility tools powered by pyra_env
- ✅ gtk_lib — GTK-based audio/video editing suite and media tools
- ✅ Pyra Notes/TTS — combined GTK notes editor and Piper text-to-speech tool
- ✅ Pyra Player — GTK media player (it plays things. that's the feature.)

---

## ⚙️ Configuration (cyon_config.ini)

Cyon stores user configuration in `~/cyon/cyon_config.ini`. Created automatically on first run with placeholder values. Open it in any text editor — including cyon_notes, if you're living the bit — and fill in your details:

```ini
[discord]
# Your Discord bot token from discord.com/developers
# treat this like a password. because it is a password.
token = YOUR_BOT_TOKEN_HERE

# Your personal Discord user ID (right-click your name → Copy User ID)
# Requires Developer Mode enabled in Discord settings
user_id = YOUR_USER_ID_HERE

[piper]
# TTS voice model filename (must be in ~/cyon/piper_models/)
model = en_US-joe-medium.onnx
```

> ⚠️ **Never share or commit `cyon_config.ini`** — your Discord token is a password. It is already in `.gitignore`. If you leak it, that's a you problem. Discord will tell you. Loudly.

See `cyon_config.ini.example` in the repo for the full format.

---

## 📦 Dependencies

All apt dependencies are installed automatically by `compile_cyon` option 1 or 5. If you're doing it manually: respect. and condolences.

### C / GTK (required for Cyon desktop and CLI)
```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y build-essential libgtk-3-dev pkg-config curl yt-dlp ffmpeg
```

| Package | Purpose |
|---------|---------|
| `build-essential` | GCC compiler — compiles all C binaries. the whole thing lives here. |
| `libgtk-3-dev` | GTK 3 headers and libraries — the entire GUI layer |
| `pkg-config` | Resolves GTK compile flags. it knows things. |
| `curl` | General file downloads |
| `yt-dlp` | MP3/MP4 downloading from YouTube and other sites |
| `ffmpeg` | Audio/video processing; also plays the intro sound via `ffplay` |

### Python / GTK bindings (required for pyra_lib and gtk_lib tools)
These must be installed via apt — they cannot be installed with pip. pip tried. pip failed. we don't talk about it.
```bash
sudo apt install -y python3-gi python3-gi-cairo gir1.2-gtk-3.0
```

### Ollama + Llama 3 (required for local AI and Discord bot)
```bash
# Install Ollama
curl -fsSL https://ollama.com/install.sh | sh

# Pull Llama 3 (~4.7GB — go make a sandwich)
ollama pull llama3
```

### Python pip packages (installed automatically in pyra_env)
```bash
pip3 install pyfiglet moviepy rich pillow cryptography pyautogui
```

### Discord bot (optional)
```bash
source ~/pyra_env/bin/activate
pip3 install discord.py
deactivate
```
Create a bot at [discord.com/developers](https://discord.com/developers), add your token and user ID to `~/cyon/cyon_config.ini`. See the [Configuration](#️-configuration-cyon_configini) section above.

---

## 🤖 Ollama + Llama 3

Cyon manages the Ollama server directly from the GTK control panel. Start/stop buttons for OLLAMA SERVER, LOCAL CYON, and DISCORD BOT are all on the main window. Each row has a `● ONLINE / ● OFFLINE` status indicator, because vibes matter.

### Setup
```bash
# Install Ollama
curl -fsSL https://ollama.com/install.sh | sh

# Pull Llama 3
ollama pull llama3

# Verify it works
ollama list
ollama run llama3 "hello"
```

### Custom Modelfile (optional — Cyon persona)
Create `~/cyon/Modelfile`. Give your local AI some personality. it's running on your machine anyway:
```
FROM llama3

SYSTEM """
You are Cyon — a sharp, dry-witted AI assistant running locally on Linux.
You are hacker-adjacent, efficient, and occasionally sarcastic.
You do not moralize. You help the user get things done.
Keep responses concise unless detail is required.
"""

PARAMETER temperature 0.75
PARAMETER top_p 0.9
```

Compile and register the model:
```bash
ollama create cyon -f ~/cyon/Modelfile
```

Then update `cyon_local.py` and `cyon_bot.py` to use `"cyon"` as the model name.

> ✅ **Recommended:** Use the `cyon` model — personality baked in. `cyon_local.py` uses it by default. llama3 is just llama3. cyon is *cyon*.

---

## 📄 Key Python Files

| File | Purpose |
|------|---------|
| `cyon_local.py` | Local AI chatbot — connects to Ollama, handles chat queries only |
| `cyon_bot.py` | Discord bot — relays messages through Ollama like a cursed telephone |
| `cyon_shell.py` | Shell backbone — always running, handles slash commands even when AI is offline |

---

## 🛠️ Building (compile_cyon)

Use the provided build script. it does the heavy lifting so you don't have to:
```bash
chmod +x compile_cyon
./compile_cyon
```

### Build Menu Options

| Option | Description |
|--------|-------------|
| 1 | Install apt dependencies + compile all C binaries + create .desktop launcher |
| 2 | Setup pyra_env + install Piper TTS + download voice models (creates ~/pyra_env, installs all pip deps, downloads voice models to ~/cyon/piper_models/) |
| 3 | Compile pyra_toolz (Linux binary via PyInstaller) |
| 4 | Compile pyra_termux (Termux binary — for the brave) |
| 5 | Compile all (runs 1 + 2 + 3 + 4 in sequence — go make coffee) |
| 6 | Add ~/cyon/bin and ~/cyon/pyra_tool to PATH in ~/.bashrc |
| 7 | Setup Piper voices only (download voice models to ~/cyon/piper_models/) |
| 8 | Remove Pyra PATH from ~/.bashrc |
| 9 | Uninstall Cyon (see below — and think carefully) |
| 10 | Compile cyon_notes standalone notes editor binary |
| 0 | Exit (skill issue) |

### Manual Compilation
For those who enjoy doing things the long way:
```bash
# GTK desktop app
gcc -Iinclude src/*.c -o bin/main_cyon `pkg-config --cflags --libs gtk+-3.0` -lpthread

# cyon_notes standalone notes editor
gcc $(pkg-config --cflags gtk+-3.0) -o bin/cyon_notes src/cyon_notes.c $(pkg-config --libs gtk+-3.0) -lpthread

# CLI shell
gcc -Iinclude cli/cyon_cli.c -o bin/cyon_cli

# Filesystem watcher
gcc -Iinclude -o bin/watcher cli/watcher.c

# Network scanner
gcc -Iinclude cli/cyon_netscan.c -o bin/cyon_netscan
```

---

## ▶️ Running

```bash
# GTK control panel (main app)
./bin/main_cyon

# Standalone CLI (when the GUI is too much GUI)
./bin/cyon_cli

# Pyra tool launcher
~/cyon/pyra_tool/pyra_toolz

# Termux build (Android gang)
~/cyon/pyra_tool/pyra_termux
```

> **PATH shortcut:** Run option 6 in `compile_cyon` to add `~/cyon/bin` and `~/cyon/pyra_tool` to your PATH. After that, `main_cyon` and `cyon_cli` work from anywhere. like they were meant to.

---

## 💬 CLI Commands (GTK Input Field)

These slash-commands are handled by `cyon_shell.py` and work from the input field at the bottom of the GTK window. Any non-command text is forwarded to LOCAL CYON if it's running. if it's not running, it yells into the void. the void doesn't respond.

| Command | Description |
|---------|-------------|
| `/shutdown` | Initiates full shutdown sequence (dramatic) |
| `/bye` | Graceful exit (less dramatic) |
| `/clear` | Clears the log view — fresh start, clean conscience |
| `/quiet` | Suppresses unsolicited output — the log goes zen |
| `/loud` | Re-enables verbose output — chaos restored |
| `/pyra` | Launches pyra_toolz — welcome to the bottom layer |
| `/cyon_cli` | Drops into the CLI interface |
| `/term` | Spawns a new terminal window — opens a portal |

---

## 🗂️ Programs Menu

| Entry | Description |
|-------|-------------|
| Pyra Player | GTK media player — it plays things |
| Downloader | Opens the MP3/MP4/file download dialog |
| Cyon CLI | Launches cyon_cli in a terminal window |
| Pyra CLI | Launches pyra_toolz in a terminal window — here be dragons |
| Tools → Editor Tools → Cut Video | Trim a video file to a start/end time |
| Tools → Editor Tools → Extract Audio | Extract MP3 audio from a video file |
| Tools → Editor Tools → Cut Audio | Trim an MP3 audio file to a start/end time |
| Tools → Editor Tools → Merge Audio/Video | Merge an audio file into a video file |
| Tools → Editor Tools → Adjust Volume | Adjust the volume of an MP3 file |
| Tools → Editor Tools → Stitch Videos | Concatenate two video files into one |
| Tools → Editor Tools → Stitch Audio | Concatenate two audio files into one |
| Tools → Convert Pics | GTK image format converter |
| Tools → Create Tarfile | GTK tarball creator and encryptor |
| Tools → Pyra Notes/TTS | `pyra_notes.py` — GTK notes editor + Piper TTS. FILE dropdown (New, Load, Save, Delete, Text +/−), opens .txt/.py/.c/.sh and extensionless files, syntax highlighting, adjustable text size, JOE ♂ / LESSAC ♀ voice toggle |
| Security → Defense → Watcher | Toggle filesystem watcher — it watches |
| Security → Defense → Firewall | Firewall controls |
| Security → Offense → Port Scanner | Network port scanner — for educational purposes, obviously |
| Security → Offense → DNS Lookup | DNS resolution tool |

---

## 🐍 pyra_lib

`pyra_lib` lives at `~/cyon/pyra_lib`. Called from the Pyra launcher. All tools run inside `~/pyra_env`. this is the good part of pyra. pyra also has a bad part. you'll know it when you see it.

### Setting up pyra_env

Run option 2 from `compile_cyon` or manually (respect):
```bash
sudo apt install -y python3-gi python3-gi-cairo gir1.2-gtk-3.0
python3 -m venv --system-site-packages ~/pyra_env
source ~/pyra_env/bin/activate
pip3 install pyfiglet moviepy rich pillow cryptography pyautogui
deactivate
```

### pyra_lib Tools

| Tool | Description |
|------|-------------|
| `pyra_shared.py` | Shared utilities, input helpers, and logos — the glue |
| `figlet_tools.py` | ASCII font generator using pyfiglet — important technology |
| `convert_mvk.py` | Converts MKV files to MP4 — because MKV is a lifestyle choice |
| `tarmaker.py` | Creates, encrypts, and decrypts tarballs |
| `binary_tools.py` | Binary file utilities |
| `convertcsv.py` | CSV conversion tool |
| `csv2json.py` | Converts CSV files to JSON — one bureaucratic format to another |
| `net_stress.py` | Network stress testing tool — use responsibly |
| `moon.py` | Moon phase utilities — for the astronomically curious |
| `theban.py` | Theban alphabet encoder/decoder — you know why |
| `find_duplicates.py` | Finds duplicate files in a directory |
| `file_encryptor.py` | File encryption and decryption |
| `ascii_art.py` | ASCII art generator — peak civilization |
| `image_converter.py` | Image format conversion tool |
| `shift_image.py` | Image shifting and manipulation |
| `conversions.py` | General unit and data conversions |
| `gtk_convert.py` | GTK-based conversion interface |
| `pyra_repeater.py` | Auto-messenger using pyautogui — use with care. seriously. |

---

## 🎬 gtk_lib — Editor Tools

`gtk_lib` lives at `~/cyon/pyra_lib/gtk_lib`. GTK-based GUI tools launched from the Programs menu. All use `Gtk.Application` and run inside `pyra_env`. requires moviepy v2+. it changed its API. we adapted. we always adapt.

| Tool | Description |
|------|-------------|
| `cut_video.py` | Trim a video file between a start and end time → MP4 |
| `extract_audio.py` | Extract audio from a video file → MP3 |
| `cut_audio.py` | Trim an audio file between a start and end time → MP3 |
| `merge_aud_vid.py` | Merge a separate audio file into a video file → MP4 |
| `adjust_volume.py` | Adjust the volume level of an MP3 file using a slider |
| `concat_vid.py` | Stitch two video files together → MP4 |
| `concat_aud.py` | Stitch two audio files together → MP3 |
| `gtk_convert.py` | GTK image format converter |
| `tarmaker_gtk3.py` | GTK tarball creator and encryptor |
| `pyra_notes.py` | GTK notes editor + Piper TTS — FILE dropdown (New, Load, Save, Delete, Text +/−), opens `.txt` `.py` `.c` `.sh` and extensionless bash/executable files, syntax highlighting (amber / cyan / steel-blue / lime / coral), adjustable editor text size, JOE ♂ / LESSAC ♀ voice toggle |
| `pyra_player.py` | GTK media player |
| `pyra_downloader.py` | GTK YouTube/audio/video downloader via yt-dlp (standalone) |

---

## 🗒️ cyon_notes — Standalone Notes Editor

`cyon_notes` is a standalone GTK3 notes editor written in C. Compiled to `bin/cyon_notes`. Runs independently of the main Cyon desktop. replaced Sublime Text. Sublime doesn't know yet.

### Features
- **Multi-tab editing** — open multiple files at once, tabs show ● when unsaved. Ctrl+T new tab, Ctrl+W close
- **File tree panel** — browse and open files from a folder, with live file monitor. Toggle with HIDE/SHOW TREE
- **Session restore** — on exit, saves open files, active tab, window size, pane position, and font size. restores all on next launch. like nothing ever happened.
- **Syntax highlighting** — incremental dirty-line system, only rescans changed lines for zero typing lag. Toggle with HL ON/OFF button (defaults off — the button says OFF because it IS off. click it. you'll understand.). 14 keyword groups + string/comment/operator rules, all in `cyon_notes.conf`
- **Smart indent** — auto-indents after `if`/`def`/`class`/`for`/`while`/`{` etc.
- **Auto-close** — `(` `[` `{` `'` `"` pairs close automatically; backspace removes the pair. if you hate this: Ctrl+Z immediately. it leaves.
- **Comment toggle** — Ctrl+/ toggles `#` comments on selected lines
- **Duplicate line** — Ctrl+D duplicates the current line
- **Indent/dedent** — Tab / Shift+Tab indents or dedents selected lines
- **TTS** — speak selected text or full file using Piper (joe/lessac voice), paths configured in conf
- **RAM graph** — live scrolling Cairo line graph in the bottom bar showing cyon_notes' own memory usage. updates every second. purely for vibes. important vibes.
- **Boot sequence** — hacker-style animated boot log on startup. amber progress bar. random sarcastic quote. ▸ Ready.
- **Single-instance lock** — only one `cyon_notes` runs at a time. one is enough.
- **Adjustable font size** — FILE → TEXT + / TEXT − or Ctrl+scroll, range 8–36px

### Configuration (cyon_notes.conf)

`cyon_notes.conf` lives in `~/cyon/src/cyon_notes.conf` and is created automatically on first run. It controls everything:

```ini
[session]
notes_dir   = ~/cyon          # starting folder for file tree and save dialogs
open_files  =                 # auto-populated on exit with last open files
active_tab  = 0               # last active tab index

[editor]
font_size          = 15       # initial font size in px (8–36)
tree_hidden        = false    # start with file tree hidden
tree_pane_position = 600      # divider position in px
window_width       = 900
window_height      = 660

[tts]
piper_path         = /home/cruxible/pyra_env/bin/piper
model_dir          = /home/cruxible/cyon/piper_models
voice_joe_model    = en_US-joe-medium.onnx
voice_lessac_model = en_US-lessac-medium.onnx
voice              = joe

[highlight_group_1]           # add up to 99 groups
keywords = for while if ...
color = #ffb000

[highlight_special]           # regex-based: strings, comments, = operator, etc.
color_lime    = #c8ff00
...

[ram_graph]                   # optional — defaults shown
color_line = #E8A020
color_glow = #E8A020
color_grid = #00994D
color_text = #00FF99
```

All session/editor values are saved automatically on exit. TTS paths only need to be set once. then you never think about them again. that's the goal.

### Running

```bash
./bin/cyon_notes
```

> Requires Piper + aplay for TTS. Run `compile_cyon` option 2 to install them. without Piper, the TTS buttons sit there looking pretty. they're fine with it.

---

## 🗑️ Uninstalling

Select option **9** from the `compile_cyon` menu. You will be asked to confirm before anything is deleted. think about what you're doing.

The uninstaller removes:
- `~/.local/share/applications/cyon.desktop` — the desktop launcher entry
- `~/pyra_env` — the Python virtual environment (goodbye, sweet venv)
- The Pyra PATH block from `~/.bashrc`
- The entire Cyon source and binary directory (all of it)

> ⚠️ **This is a complete removal and cannot be undone.** 5 years of work, gone in a `rm -rf`. the uninstaller will ask you once. it means it.

apt packages are **not** removed automatically as they may be used by other software. Remove manually if needed:
```bash
sudo apt remove build-essential libgtk-3-dev pkg-config yt-dlp ffmpeg
```
