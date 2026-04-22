# 🧠 CYON DEVLOG
### notes from the guy building it

> This isn't documentation. Documentation is in README.md and cyon_docs.html.
> This is the other thing — what I actually learned, what broke, what surprised me,
> what I'd do differently, and what I still don't fully understand.

---

## Where this started

I didn't set out to build a desktop environment. I wanted a GTK window that would run Ollama and a Discord bot without me having to open a terminal every time. Then I thought it'd be cool to have a custom launcher. Then a file manager. Then somewhere in there I wrote a window manager in C and I'm not sure how that happened.

Cyon is built around one idea: if it runs on Linux and I use it, I want to understand exactly how it works and control exactly what it does. No Electron. No web tech pretending to be a desktop. No 400MB runtime for a notes app.

The cost of that is that I've written a lot of things that already exist and are better than my versions. But I understand mine.

---

## Things I didn't expect to learn

**X11 is both simpler and weirder than I expected.** The protocol is old but it's coherent. `XCreateWindow`, `XMapWindow`, `XSelectInput`, event loop — it makes sense. What doesn't make sense is how much of it is just... convention. There's no spec for "how to make a window manager that plays nice with other software." You just figure out what ICCCM says, ignore the parts that don't work, and hope other apps aren't doing something crazy.

**GTK3 CSS is genuinely powerful** but the priority system is subtle. Two providers loaded at `GTK_STYLE_PROVIDER_PRIORITY_APPLICATION` fight each other based on load order, not specificity. The fix — scoping rules to class names, or using `:not(.class)` — works, but it took a while to understand why the palette buttons in cyon_pixelart kept going dark blue when they should have been red. The global `button {}` rule in gtk.css was winning. The fix was `.color-btn` exclusion.

**VTE is the right answer for a terminal.** I spent a long time building a fake terminal — a GtkTextView, a GtkEntry, manual PTY with `forkpty`, ANSI stripping, a key intercept handler for Ctrl+C/D/Z, arrow keys for history, everything. It mostly worked. But "mostly worked" for a terminal is not good enough. The `.bashrc` recursion, the prompt disappearing, Python's REPL not getting Enter — these weren't bugs I could fix from the C side. VTE handles all of it natively because it's a real terminal. Lesson: don't re-implement things that are hard for good reasons.

**CyonScript grew more than I planned.** What started as "run a few commands from a book" now has variables, math, if/else, while loops, for loops with step, comments, six built-in player vars, two execution modes (player and console), scheduled scripts, block triggers, bottle message delivery, and wireless redstone (CyFi). The interpreter is a line-by-line state machine. It's not fast but it's fast enough for Minecraft scripts. The hardest part was the math parser — expressions like `{x} + 10` inside a `tp` command, or `give diamond {total} / 2`. I ended up evaluating variable references first, then doing a simple left-to-right arithmetic pass. It works for everything I needed.

**The CyFi wireless redstone system** is one of the things I'm most happy with. The idea was: what if you could fire a redstone signal from anywhere on the map, from a script, without any physical wire? You register a named point next to your mechanism. Your script says `cyfi lights pulse 20`. Anyone with a linked button anywhere can trigger it. The implementation is just placing and removing redstone blocks — but the effect is that the whole map becomes programmable. Players can wire things together without being builders or OPs. That felt like the right design.

**The scoreboard plugin (CruxibleBoard)** hooks into three other plugins at runtime. That was my first time writing inter-plugin integration in Java. The pattern — get the plugin by name, call methods via reflection if needed — works but it's fragile. If Economy isn't loaded, the balance shows N/A. That's fine. But the ordering matters: Board has to enable after the other three. Maven's dependency system doesn't help here because these are runtime dependencies, not compile-time ones. I handle it with null checks and warning logs.

---

## Things that are still rough

**cyon_wm and multi-monitor** — I have a `screens.conf` but the WM doesn't fully use it for workspace-per-monitor setups. Windows on the second monitor sometimes get confused about which workspace they belong to. I know where the bug is (the workspace check uses the main display root instead of the output geometry) but haven't fixed it.

**cyon_neo's ANSI handling** — even with VTE, the boot messages and some program output occasionally show escape sequences as literal characters. VTE handles its own output fine; the issue is the startup text I inject with `vte_terminal_feed()`. I should probably not inject anything and just let the shell produce its own output.

**cyon_editor waveform generation** — it runs ffmpeg in a thread and decodes to raw PCM. For long videos this takes a while and the progress isn't shown. The waveform just appears when it's ready. A progress pulse during generation would be better.

**pyra_lib is a pile** — it started as "useful Python scripts I keep rewriting" and became a sprawling collection of tools that don't all follow the same conventions. Some use `rich`, some use GTK, some are CLI-only, some are half-broken. I know which ones work and use them. A future cleanup pass would group them better and remove the dead ones.

**The web dashboard** has a Minecraft server log stream that works great when everything is running but doesn't handle reconnection if the server restarts mid-session. The SSE endpoint just closes. You have to refresh the page. It's fine for my use case but it's not production quality.

---

## What I'd do differently

**Start with VTE.** Don't build a fake terminal. The amount of time I spent on the manual PTY implementation, key intercept handlers, ANSI stripping, the `\r\n` problem, the prompt disappearing — all of that went away when I switched to VTE. The fake terminal was a good learning exercise but I should have recognized earlier that it was a dead end for actual daily use.

**Design the plugin ecosystem first.** The Cruxible plugins grew independently and ended up with overlapping responsibilities. Economy, Ranking, and Jobs all needed to know about each other, and I wired them together after the fact through runtime lookups. If I'd designed the data model first — a shared player data store that all three plugins read from — the integration would have been cleaner.

**The GTK theme would be CSS variables from day one.** The cyon_editor had its colors baked in as hardcoded hex strings in the Python source. When I built the global GTK theme, I had to go back and cross-reference every color manually. Starting with a `@define-color` system at the top means changes propagate everywhere automatically.

---

## Things I'm genuinely proud of

**The streaming glitch effect on the WM menus.** When you open a dropdown in cyon_wm, the items start as scrambled characters and then "resolve" into real text. It's implemented as a timer that decrements a reveal counter per item. It's maybe 30 lines of C and it makes the whole thing feel alive.

**CyonScript's variable system in expressions.** `tp {x} + 10 {y} {z}` just works. `give diamond {total} / 2` just works. I had to expand variables first, then evaluate the math, then pass the result to the instruction handler. The fact that it composes naturally — variables in for loop bounds, math in wait durations, player vars in give amounts — makes it feel like a real language instead of a macro system.

**cyon_about rendering README.md live.** The about window reads `~/cyon/README.md` at runtime and renders it as GTK widgets — headings, body text, code blocks, bullet points, horizontal rules. It's not perfect markdown but it means the README is also the in-app documentation and they stay in sync automatically.

**The whole thing actually works as a daily driver.** I use cyon_wm as my actual desktop. cyon_files is my file manager. cyon_neo is my terminal. cyon_editor I use for video. The Cruxible plugins run on an actual Minecraft server with actual players on it. None of this is demo software. It runs.

---

## Stack summary

| Layer | Language | What it does |
|-------|----------|--------------|
| WM + native tools | C + X11 + GTK3 | Window manager, terminal, file manager, pixelart, notes |
| Python tools | Python 3 + GTK3 | Video editor, media tools, AI chat, converters |
| 3D world | Python + pygame | First-person launcher world |
| Web dashboard | Python + Flask | Browser control panel |
| Combat bot | JavaScript + Node.js | Mineflayer Minecraft bot |
| Server plugins | Java + Paper API | CyonScript, Economy, Ranking, Jobs, LandClaims, etc. |
| AI | Ollama + Llama 3 | Local LLM, no cloud |
| Build system | Bash | compile_cyon menu |

---

## Current version: v5.9 — "we swear it's intentional this time"

- Full aesthetic overhaul: hacker/CRT theme across all GTK tools — darker backgrounds, glowing greens, blinking/unstable UI energy throughout
- cyon_pixelart shipped as a compiled binary (`bin/cyon_pixelart`) — GTK pixel art editor with animation timeline now a first-class Cyon tool
- cyon_editor (video editor) integrated into the main ecosystem — GStreamer + ffmpeg, waveform display, trim/export, accessible from cyon_wm menus
- cyon_tty — Cyon's own terminal emulator, no external dependencies, ships as `bin/cyon_tty`
- main_cyon_shell — native Cyon shell binary, ships as `bin/main_cyon_shell`
- tarmaker_gtk3: GTK archive tool promoted to pyra_lib/gtk_lib, exclude pattern config moved to `~/cyon/cyon_config/cyon_tarmaker.conf`
- Updated pyra_lib tools: cyon_matrix, cyon_player, cyon_about, cyon_sc, color_picker, render.py, cyon_tts
- New pyra_lib entries: net_stress.py, mem_scan.py, binary_tools.py, cybrmsc.py, shift_image.py, convert_mvk.py
- Assets: cyon_banner variants (ember, 90, mix), new favicon, updated main_cyon.webp
- cyon_config: lotr_quotes.jsonl added; config layout consolidated under `~/cyon/cyon_config/`
- Cyon sounds expanded: lincolnshire_poacher, titanic_sos, terrydavis_lastvideo_tos, swedish_rhapsody, sonar_ping, blackhole
- cyon_world v3 (cyon_world_main_v3.py) — updated 3D launcher world

---

## v5.8

- Added VTE terminal to cyon_neo (replaced manual PTY + textview + entry)
- Added CMD right panel with programmable buttons (Label|command config)
- Replaced NEO bottom buttons (FULL/REGION CAP → CLEAR/TOP/COPY/PASTE/FONT+/FONT-)
- Added right-click context menu on VTE terminal
- cyon_wm third theme: cyon (navy + cyan), theme toggle now cycles green → ember → cyon
- Global GTK3 CSS theme system with cyan and red variants
- cyon_pixelart palette button fix (`.color-btn` exclusion from global button rule)
- CruxibleBooks v3.6: CyonScript for/endfor with step, scheduled scripts, CyFi wireless redstone, block triggers, bottle messages
- CruxibleLandClaims v2.1: particle borders, gold block corner markers, /claimsee
- CruxibleBoard: live sidebar integrating Economy + Ranking + Jobs

---

## Things on the list

- cyon_neo: tab completion feedback (show candidates when tab pressed mid-word)
- cyon_wm: proper multi-monitor workspace handling
- cyon_editor: waveform generation progress indicator
- CyonScript: `include <bookname>` instruction to compose scripts from other scripts
- CruxibleEconomy: bank interest, configurable transaction fees
- A `dev_notes.html` companion to `cyon_docs.html` that renders this file

---

*— Ioannes Cruxibulum*
