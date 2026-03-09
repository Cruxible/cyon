# 🌀 Cyon
Cyon is a hybrid GTK and CLI-based desktop tool built in C. It serves as a control panel for local AI (Ollama + Llama 3), a Discord bot manager, a media downloader, and a launcher for a suite of Python utility tools. It is lightweight, hacker-themed, and extensible.

> Currently under active development.

---

## ✨ Features

- ✅ GTK 3 control panel — manage Ollama, local AI, Discord bot, and tool engine from one window
- ✅ Local AI chatbot powered by Ollama + Llama 3 (no cloud, no API key)
- ✅ Cyon Tools — separate llama.cpp-powered tool engine with real shell, ping, whois, file, and app launch tools
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
| `cyon_tools.py` | Tool engine — llama.cpp powered, executes real shell commands, ping, whois, file checks, app launches |
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
| 8 | Remove Pyra PATH from ~/.bashrc |
| 9 | Uninstall Cyon |
| 0 | Exit |

### Manual Compilation
```bash
# GTK desktop app
gcc -Iinclude src/*.c -o bin/main_cyon `pkg-config --cflags --libs gtk+-3.0` -lpthread

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
| Tools → Pyra Notes/TTS | Combined notes editor + Piper TTS — write/save/load notes, speak selection or all text, JOE ♂ / LESSAC ♀ voice toggle |
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
| `pyra_notes.py` | GTK notes editor + Piper TTS — write/save/load notes from `~/Documents/pyra_dev_notes`, speak selection or full text, JOE ♂ / LESSAC ♀ voice toggle |
| `pyra_player.py` | GTK media player |
| `pyra_downloader.py` | GTK YouTube/audio/video downloader via yt-dlp (standalone) |

---

🗒️ Pyra Notes/TTS

Pyra Notes/TTS is a combined GTK notes editor and Piper text-to-speech tool, launched from Tools → Pyra Notes/TTS in the Programs menu. Notes are saved to and loaded from `~/Documents/pyra_dev_notes`. The TTS section lets you speak selected text or the full note using either the JOE ♂ or LESSAC ♀ Piper voice. The Discord bot `/say` command also uses Piper to generate voice replies.

⚠️ Piper and a voice model must both be installed or TTS will not work.

Easiest Setup — compile_cyon option 2

Running option 2 from compile_cyon automatically:

• Creates ~/pyra_env
• Installs Piper via pip
• Installs audio dependencies
• Downloads voice models to ~/piper_models

This is the recommended method.

Manual Setup
1. Install Piper
sudo apt update
sudo apt install -y python3-pip ffmpeg libsndfile1 alsa-utils pulseaudio-utils

source ~/pyra_env/bin/activate

pip install piper-tts
pip install --upgrade piper-tts onnxruntime pathvalidate

#### 2. Download a Voice Model
```bash
mkdir -p ~/cyon/piper_models
cd ~/cyon/piper_models

# Example: en_US-lessac-medium (male)
wget https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/lessac/medium/en_US-lessac-medium.onnx
wget https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/lessac/medium/en_US-lessac-medium.onnx.json

# Example: en_US-amy-medium (female)
wget https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/amy/medium/en_US-amy-medium.onnx
wget https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/amy/medium/en_US-amy-medium.onnx.json
```

Each Piper model consists of two files — a `.onnx` model file and a `.onnx.json` config file. Both must be present in `~/cyon/piper_models/`. Browse all available voices at [huggingface.co/rhasspy/piper-voices](https://huggingface.co/rhasspy/piper-voices).

#### 3. Update cyon_config.ini
```ini
[piper]
model = en_US-joe-medium.onnx
```

> ℹ️ The model name must exactly match the `.onnx` filename in `~/cyon/piper_models/`.

---

### Notes
- All editor tools require `moviepy` (v2+) installed in `pyra_env`
- MoviePy v2 renames: `subclip` → `subclipped`, `set_audio` → `with_audio`, volume effects now use `MultiplyVolume` — all tools reflect this
- Tools are launched via `python3` calls from `main_cyon.c` and run as background processes

---


---

## 🦙 llama.cpp — cyon_tools.py

Cyon Tools (`cyon_tools.py`) uses `llama-cpp-python` to run a local GGUF model directly — no Ollama server required. It is a completely separate process from `cyon_local.py` and is managed by its own **CYON TOOLS** row in the GTK control panel.

`cyon_local.py` uses **Ollama only** for chat. `cyon_tools.py` uses **llama.cpp** for tool execution. They run independently and can both be active at the same time.

### Step 1 — Install llama-cpp-python

Install into your existing `pyra_env`:
```bash
/home/cruxible/pyra_env/bin/pip install llama-cpp-python
```

> ⚠️ If you compiled llama.cpp manually with GPU support, see the [llama-cpp-python docs](https://github.com/abetlen/llama-cpp-python) for the correct install flags.

### Step 2 — Download a GGUF Model

Modern llama.cpp requires **GGUF format** models. The old `.bin` format is not supported.

```bash
# Install huggingface-hub into pyra_env
/home/cruxible/pyra_env/bin/pip install huggingface-hub

# Download Llama 3 8B (recommended — ~4.7GB)
/home/cruxible/pyra_env/bin/python -c "
from huggingface_hub import hf_hub_download
hf_hub_download(
    repo_id='bartowski/Meta-Llama-3-8B-Instruct-GGUF',
    filename='Meta-Llama-3-8B-Instruct-Q4_K_M.gguf',
    local_dir='/home/cruxible/cyon/llama3_models/'
)
print('Download complete!')
"
```

| Model | Size | RAM Needed | Quality |
|-------|------|-----------|---------|
| `Meta-Llama-3-8B-Instruct-Q4_K_M.gguf` | ~4.7GB | ~6GB | ✅ Recommended |
| `Meta-Llama-3-8B-Instruct-Q3_K_S.gguf` | ~3.4GB | ~4GB | Lower quality |

### Step 3 — Update cyon_tools.py

Set the model path at the top of `cyon_tools.py`:
```python
LLAMA_MODEL_PATH = "/home/cruxible/cyon/llama3_models/Meta-Llama-3-8B-Instruct-Q4_K_M.gguf"
```

### Step 4 — Verify

Start Cyon and check the output:
```
[LOCAL] Loading model, please wait...
[LOCAL] Model loaded OK.
```

If the model fails to load, Cyon will print a clear error and **continue running** — all slash commands still work without the AI.

---

## 🔧 cyon_tools.py — Tool Engine

`cyon_tools.py` is a standalone tool engine that gives Cyon the ability to run real commands on your system in response to natural language. It runs as a separate process from `cyon_local.py` and is managed by the **CYON TOOLS** START/STOP row in the GTK window.

### How It Works

When you type a message, Cyon passes it to the local LLM. If the model determines a tool is needed, it responds with a `TOOL:` line in this exact format:

```
TOOL: tool_name argument
```

`cyon_local.py` intercepts that line, runs the tool, and includes the result in the response.

### Built-in Tools

| Tool | Usage | Description |
|------|-------|-------------|
| `shell` | `TOOL: shell <command>` | Run any bash command and return output |
| `file_check` | `TOOL: file_check <path>` | Check if a file exists and get its size |
| `ping` | `TOOL: ping <host>` | Ping a host or IP address |
| `whois` | `TOOL: whois <domain>` | Look up domain registration info |
| `launch` | `TOOL: launch <app>` | Launch an app in a new terminal window |

### Launch Tool — Supported Apps

| App Name | Launches |
|----------|----------|
| `pyra_toolz` or `pyra` | `~/cyon/pyra_tool/pyra_toolz` |
| `cyon_cli` | `~/cyon/bin/cyon_cli` |

### Example Prompts

```
what processes are running on my system
how much disk space do i have left
check if the file ~/cyon/cyon_local.py exists
ping google.com
run pyra_toolz
```

### Adding Your Own Tools

Open `cyon_local.py` and add a function to the `TOOLS` dict:

```python
def tool_mycommand(arg):
    import subprocess
    result = subprocess.check_output(["my_command", arg], timeout=10)
    return result.decode("utf-8")[:500]

TOOLS = {
    ...
    "mycommand": tool_mycommand,
}
```

Then tell Cyon about it by adding an example to the `SYSTEM_PROMPT`.

### Slash Commands (always available even without AI)

These work whether or not the LLM is loaded:

| Command | Description |
|---------|-------------|
| `/shutdown` | Shut down the PC |
| `/bye` | Exit Cyon gracefully |
| `/clear` | Clear conversation history |
| `/pyra` | Launch pyra_toolz in a new terminal |
| `/cyon_cli` | Launch Cyon CLI in a new terminal |
| `/term` | Open a new terminal window |
| `/status` | Show model load status and history length |

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