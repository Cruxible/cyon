# 🌀 Cyon
Cyon is a hybrid GTK and CLI-based desktop tool built in C. It serves as a control panel for local AI (Ollama + Llama 3), a Discord bot manager, a media downloader, and a launcher for a suite of Python utility tools. It is lightweight, hacker-themed, and extensible.

> Currently under active development.

---

## ✨ Features

- ✅ GTK 3 control panel — manage Ollama, local AI, and Discord bot from one window
- ✅ Local AI chatbot powered by Ollama + Llama 3 (no cloud, no API key)
- ✅ Discord bot integration — relay messages through local Llama 3
- ✅ MP3/MP4 and file downloading using yt-dlp and curl
- ✅ Standalone CLI with styled shell prompt
- ✅ Shell command processor — slash-commands wired directly to the GTK input field
- ✅ Modular design for extending tools and utilities
- ✅ Linux-focused
- ✅ Pyra tool launcher — launches pyra_toolz directly from the Programs menu
- ✅ pyra_lib — a collection of Python utility tools powered by pyra_env
- ✅ gtk_lib — GTK-based audio/video editing suite and media tools
- ✅ Pyra Notes/TTS — combined GTK notes editor and Piper text-to-speech tool
- ✅ Pyra Player — GTK media player

---

## ⚙️ Configuration (cyon_config.ini)

Cyon stores user configuration in `~/cyon/cyon_config.ini`. This file is created automatically on first run with placeholder values. Open it in any text editor and fill in your details:

```ini
[discord]
# Your Discord bot token from discord.com/developers
token = YOUR_BOT_TOKEN_HERE

# Your personal Discord user ID (right-click your name → Copy User ID)
# Requires Developer Mode enabled in Discord settings
user_id = YOUR_USER_ID_HERE

[piper]
# TTS voice model filename (must be in ~/cyon/piper_models/)
model = en_US-joe-medium.onnx
```

> ⚠️ **Never share or commit `cyon_config.ini`** — your Discord token is a password. It is already in `.gitignore`.

See `cyon_config.ini.example` in the repo for the full format.

---

## 📦 Dependencies

All apt dependencies are installed automatically by `compile_cyon` option 1 or 5.

### C / GTK (required for Cyon desktop and CLI)
```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y build-essential libgtk-3-dev pkg-config curl yt-dlp ffmpeg
```

| Package | Purpose |
|---------|---------|
| `build-essential` | GCC compiler — compiles all C binaries |
| `libgtk-3-dev` | GTK 3 headers and libraries — the entire GUI layer |
| `pkg-config` | Resolves GTK compile flags |
| `curl` | General file downloads |
| `yt-dlp` | MP3/MP4 downloading from YouTube and other sites |
| `ffmpeg` | Audio/video processing; also plays the intro sound via `ffplay` |

### Python / GTK bindings (required for pyra_lib and gtk_lib tools)
These must be installed via apt — they cannot be installed with pip:
```bash
sudo apt install -y python3-gi python3-gi-cairo gir1.2-gtk-3.0
```

### Ollama + Llama 3 (required for local AI and Discord bot)
```bash
# Install Ollama
curl -fsSL https://ollama.com/install.sh | sh

# Pull Llama 3
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
Create a bot at [discord.com/developers](https://discord.com/developers), then add your token and user ID to `~/cyon/cyon_config.ini`. See the [Configuration](#️-configuration-cyon_configini) section above.

---

## 🤖 Ollama + Llama 3

Cyon manages the Ollama server directly from the GTK control panel. Start/stop buttons for OLLAMA SERVER, LOCAL CYON, and DISCORD BOT are on the main window. Each row has a `● ONLINE / ● OFFLINE` status indicator.

### Setup
```bash
# Install Ollama
curl -fsSL https://ollama.com/install.sh | sh

# Pull Llama 3
ollama pull llama3

# Verify
ollama list
ollama run llama3 "hello"
```

### Custom Modelfile (optional — Cyon persona)
Create `~/cyon/Modelfile`:
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

> ✅ **Recommended:** Use the `cyon` model — it has Cyon's personality baked in. `cyon_local.py` is configured to use `"cyon"` by default.

---

## 📄 Key Python Files

| File | Purpose |
|------|---------|
| `cyon_local.py` | Local AI chatbot — connects to Ollama, handles chat queries only |
| `cyon_bot.py` | Discord bot — relays messages through Ollama |
| `cyon_shell.py` | Shell backbone — always running, handles slash commands even when AI is offline |

---

## 🛠️ Building (compile_cyon)

Use the provided build script for all compilation and environment setup:
```bash
chmod +x compile_cyon
./compile_cyon
```

### Build Menu Options

| Option | Description |
|--------|-------------|
| 1 | Install apt dependencies + compile all C binaries + create .desktop launcher |
| 2 | Setup pyra_env + install Piper TTS + download voice models (creates ~/pyra_env, installs all pip deps into it, downloads voice models to ~/cyon/piper_models/) |
| 3 | Compile pyra_toolz (Linux binary via PyInstaller) |
| 4 | Compile pyra_termux (Termux binary) |
| 5 | Compile all (runs 1 + 2 + 3 + 4 in sequence) |
| 6 | Add ~/cyon/bin and ~/cyon/pyra_tool to PATH in ~/.bashrc |
| 7 | Setup Piper voices only (download voice models to ~/cyon/piper_models/) |
| 10 | Compile cyon_notes standalone notes editor binary |
| 8 | Remove Pyra PATH from ~/.bashrc |
| 9 | Uninstall Cyon |
| 0 | Exit |

### Manual Compilation
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

# Standalone CLI
./bin/cyon_cli

# Pyra tool launcher
~/cyon/pyra_tool/pyra_toolz

# Termux build
~/cyon/pyra_tool/pyra_termux
```

> **PATH shortcut:** Run option 6 in `compile_cyon` to add `~/cyon/bin` and `~/cyon/pyra_tool` to your PATH. After that, `main_cyon` and `cyon_cli` work from anywhere.

---

## 💬 CLI Commands (GTK Input Field)

These slash-commands are handled by `cyon_shell.py` and work from the input field at the bottom of the GTK window. Any non-command text is forwarded to LOCAL CYON if it is running.

| Command | Description |
|---------|-------------|
| `/shutdown` | Initiates full shutdown sequence |
| `/bye` | Graceful exit |
| `/clear` | Clears the log view |
| `/quiet` | Suppresses unsolicited output |
| `/loud` | Re-enables verbose output |
| `/pyra` | Launches pyra_toolz |
| `/cyon_cli` | Drops into the CLI interface |
| `/term` | Spawns a new terminal window |

---

## 🗂️ Programs Menu

| Entry | Description |
|-------|-------------|
| Pyra Player | GTK media player |
| Downloader | Opens the MP3/MP4/file download dialog |
| Cyon CLI | Launches cyon_cli in a terminal window |
| Pyra CLI | Launches pyra_toolz in a terminal window |
| Tools → Editor Tools → Cut Video | Trim a video file to a start/end time |
| Tools → Editor Tools → Extract Audio | Extract MP3 audio from a video file |
| Tools → Editor Tools → Cut Audio | Trim an MP3 audio file to a start/end time |
| Tools → Editor Tools → Merge Audio/Video | Merge an audio file into a video file |
| Tools → Editor Tools → Adjust Volume | Adjust the volume of an MP3 file |
| Tools → Editor Tools → Stitch Videos | Concatenate two video files into one |
| Tools → Editor Tools → Stitch Audio | Concatenate two audio files into one |
| Tools → Convert Pics | GTK image format converter |
| Tools → Create Tarfile | GTK tarball creator and encryptor |
| Tools → Pyra Notes/TTS | **cyon_notes** — standalone GTK notes editor (C binary). Multi-tab editing, file tree panel, syntax highlighting via `cyon_notes.conf`, session restore (open files, active tab, window size), configurable TTS paths, adjustable font size, JOE ♂ / LESSAC ♀ voice toggle, single-instance lock |
| Security → Defense → Watcher | Toggle filesystem watcher |
| Security → Defense → Firewall | Firewall controls |
| Security → Offense → Port Scanner | Network port scanner |
| Security → Offense → DNS Lookup | DNS resolution tool |

---

## 🐍 pyra_lib

`pyra_lib` lives at `~/cyon/pyra_lib` and is called from the Pyra launcher. All tools run inside `~/pyra_env`.

### Setting up pyra_env

Run option 2 from `compile_cyon` or manually:
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
| `pyra_shared.py` | Shared utilities, input helpers, and logos used across pyra_lib |
| `figlet_tools.py` | ASCII font generator using pyfiglet |
| `convert_mvk.py` | Converts MKV files to MP4 |
| `tarmaker.py` | Creates, encrypts, and decrypts tarballs |
| `binary_tools.py` | Binary file utilities |
| `convertcsv.py` | CSV conversion tool |
| `csv2json.py` | Converts CSV files to JSON |
| `net_stress.py` | Network stress testing tool |
| `moon.py` | Moon phase utilities |
| `theban.py` | Theban alphabet encoder/decoder |
| `find_duplicates.py` | Finds duplicate files in a directory |
| `file_encryptor.py` | File encryption and decryption |
| `ascii_art.py` | ASCII art generator |
| `image_converter.py` | Image format conversion tool |
| `shift_image.py` | Image shifting and manipulation |
| `conversions.py` | General unit and data conversions |
| `gtk_convert.py` | GTK-based conversion interface |
| `pyra_repeater.py` | Auto-messenger using pyautogui — use with care |

---

## 🎬 gtk_lib — Editor Tools

`gtk_lib` lives at `~/cyon/pyra_lib/gtk_lib` and contains GTK-based GUI tools launched from the Programs menu. All tools use `Gtk.Application` and run inside `pyra_env`.

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
| `cyon_notes` (C binary) | Standalone GTK notes editor — multi-tab, file tree, session restore, syntax highlighting via `cyon_notes.conf` (14 keyword groups + special rules), TTS via Piper (joe/lessac), all paths/settings in conf, single-instance lock |
| `pyra_player.py` | GTK media player |
| `pyra_downloader.py` | GTK YouTube/audio/video downloader via yt-dlp (standalone) |

---

## 🗒️ cyon_notes — Standalone Notes Editor

`cyon_notes` is a standalone GTK3 notes editor written in C. It is compiled to `bin/cyon_notes` and run independently of the main Cyon desktop.

### Features
- **Multi-tab editing** — open multiple files at once, tabs show `*` when unsaved
- **File tree panel** — browse and open files from a folder, with live file monitor
- **Session restore** — on exit, saves open files, active tab, window size, pane position, and font size; restores all on next launch
- **Syntax highlighting** — 14 keyword color groups + string/comment/operator rules, all defined in `cyon_notes.conf`
- **Smart indent** — auto-indents after `if`/`def`/`class`/`for`/`while`/`{` etc.
- **Auto-close** — `(` `[` `{` `'` `"` pairs close automatically; backspace removes the pair
- **TTS** — speak selected text or full file using Piper (joe/lessac voice), paths configured in conf
- **Single-instance lock** — only one `cyon_notes` runs at a time
- **Adjustable font size** — FILE → TEXT + / TEXT −, range 8–36px

### Configuration (cyon_notes.conf)

`cyon_notes.conf` lives in the same directory as the binary and is created automatically on first run. It controls everything:

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
```

All session/editor values are saved automatically on exit. TTS paths only need to be set once.

### Running

```bash
./bin/cyon_notes
```

> Requires Piper + aplay for TTS. Run `compile_cyon` option 2 to install them.



---

## 🗑️ Uninstalling

Select option **9** from the `compile_cyon` menu. You will be asked to confirm before anything is deleted.

The uninstaller removes:
- `~/.local/share/applications/cyon.desktop` — the desktop launcher entry
- `~/pyra_env` — the Python virtual environment
- The Pyra PATH block from `~/.bashrc`
- The entire Cyon source and binary directory

> ⚠️ **This is a complete removal and cannot be undone.**

apt packages are **not** removed automatically as they may be used by other software. To remove them manually:
```bash
sudo apt remove build-essential libgtk-3-dev pkg-config yt-dlp ffmpeg
```