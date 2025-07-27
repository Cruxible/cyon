# 🌀 Cyon

**Cyon** is a hybrid GTK and CLI-based desktop tool built in C, designed to provide fast and simple utilities like file downloading (MP3, MP4, general files), custom tools, and a styled terminal interface for extended control. It is lightweight, hacker-themed, and extensible.

---

## ✨ Features

- ✅ GTK 3 GUI with buttons, dialogs, and download functionality  
- ✅ MP3/MP4 and file downloading using `yt-dlp` and `curl`  
- ✅ Standalone CLI with styled shell prompt  
- ✅ Modular design for extending tools and utilities  
- ✅ Save notes from the GUI  
- ✅ Cross-platform (Linux-focused)

---

## 📦 Dependencies

Install the following packages before compiling:

```bash
sudo apt update
sudo apt install build-essential libgtk-3-dev pkg-config curl

🛠️ Building
Compile Cyon Desktop:

gcc -Iinclude src/*.c -o main_cyon `pkg-config --cflags --libs gtk+-3.0`
chmod +x main_cyon

Compile Cyon CLI:

gcc -Iinclude cli/cyon_cli.c -o cyon_cli
chmod +x cyon_cli

Or use the provided helper script:

chmod +x compile_cyon
./compile_cyon

▶️ Running
GUI (Main Application):

./main_cyon

CLI (Standalone or from GUI Tools Dropdown):

./cyon_cli
