import sys
import subprocess
import os

# ---- CONFIG ----
LLAMA_MODEL_PATH = (
    "/home/cruxible/cyon/llama3_models/Meta-Llama-3-8B-Instruct-Q4_K_M.gguf"
)

# Piper TTS paths
PIPER_EXE = "/home/cruxible/pyra_env/bin/piper"
PIPER_MODEL = "/home/cruxible/cyon/piper_models/en_US-joe-medium.onnx"
PIPER_CONFIG = "/home/cruxible/cyon/piper_models/en_US-joe-medium.onnx.json"
VOICE_OUTPUT = "/home/cruxible/cyon/piper_models/voice_tools.wav"

# ---- Conversation history ----
conversation_history = []

SYSTEM_PROMPT = """You are Cyon, a local AI assistant with access to system tools. Be concise and direct.

To run a command or use a tool, you MUST use exactly this format:
TOOL: tool_name argument

Examples:
User: what processes are running?
Assistant: TOOL: shell ps aux --no-headers | awk '{print $11}' | sort -u | head -20

User: check if a file exists at /home/cruxible/test.txt
Assistant: TOOL: file_check /home/cruxible/test.txt

User: how much disk space do i have?
Assistant: TOOL: shell df -h /

User: run pyra_toolz
Assistant: TOOL: launch pyra_toolz

User: ping google.com
Assistant: TOOL: ping google.com

User: whois github.com
Assistant: TOOL: whois github.com

Available tools:
TOOL: shell <any bash command>
TOOL: file_check <path>
TOOL: ping <host>
TOOL: whois <domain>
TOOL: launch <app_name>  (apps: pyra_toolz, cyon_cli)

Rules:
- NEVER write PROCESS:, CMD:, or anything other than TOOL:
- NEVER explain yourself or add commentary
- NEVER write code blocks
- One response only, no sign-offs
"""

# ---- Safely initialize LLaMA ----
llm = None


def init_llama():
    global llm
    try:
        from llama_cpp import Llama

        if not os.path.exists(LLAMA_MODEL_PATH):
            print(
                f"[TOOLS] ERROR: Model file not found: {LLAMA_MODEL_PATH}", flush=True
            )
            print("[TOOLS] Please update LLAMA_MODEL_PATH in cyon_tools.py", flush=True)
            return False
        print("[TOOLS] Loading model, please wait...", flush=True)
        llm = Llama(model_path=LLAMA_MODEL_PATH, n_ctx=2048, n_threads=4, verbose=False)
        print("[TOOLS] Model loaded OK.", flush=True)
        return True
    except ImportError:
        print(
            "[TOOLS] ERROR: llama_cpp not installed. Run: pip install llama-cpp-python",
            flush=True,
        )
        return False
    except Exception as e:
        print(f"[TOOLS] ERROR loading model: {e}", flush=True)
        return False


# ---- TOOLS ----
def tool_whois(domain):
    try:
        result = subprocess.check_output(
            ["whois", domain], timeout=10, stderr=subprocess.DEVNULL
        )
        return result.decode("utf-8", errors="ignore")[:500]
    except FileNotFoundError:
        return "[TOOL] whois not installed. Try: sudo apt install whois"
    except Exception as e:
        return f"[TOOL] whois error: {e}"


def tool_ping(host):
    try:
        result = subprocess.check_output(
            ["ping", "-c", "3", host], timeout=10, stderr=subprocess.DEVNULL
        )
        return result.decode("utf-8", errors="ignore")
    except Exception as e:
        return f"[TOOL] ping error: {e}"


def tool_file_check(path):
    path = os.path.expanduser(path)
    if os.path.exists(path):
        size = os.path.getsize(path)
        return f"[TOOL] File exists: {path} ({size} bytes)"
    return f"[TOOL] File not found: {path}"


def tool_shell(command):
    try:
        result = subprocess.check_output(
            command, shell=True, timeout=15, stderr=subprocess.STDOUT
        )
        return result.decode("utf-8", errors="ignore")[:500]
    except subprocess.TimeoutExpired:
        return "[TOOL] Command timed out."
    except Exception as e:
        return f"[TOOL] Shell error: {e}"


def tool_launch(app_name):
    app_paths = {
        "pyra_toolz": os.path.expanduser("~/cyon/pyra_tool/pyra_toolz"),
        "pyra": os.path.expanduser("~/cyon/pyra_tool/pyra_toolz"),
        "cyon_cli": os.path.expanduser("~/cyon/bin/cyon_cli"),
    }
    path = app_paths.get(app_name.lower().strip())
    if not path:
        return (
            f"[TOOL] Unknown app: {app_name}. Known apps: {', '.join(app_paths.keys())}"
        )
    if not os.path.exists(path):
        return f"[TOOL] App not found at: {path}"
    terminals = [
        "mate-terminal",
        "x-terminal-emulator",
        "gnome-terminal",
        "xfce4-terminal",
        "lxterminal",
        "konsole",
        "xterm",
    ]
    for term in terminals:
        try:
            subprocess.Popen([term, "-e", path])
            return f"[TOOL] Launched {app_name} in a new terminal."
        except FileNotFoundError:
            continue
    return "[TOOL] No compatible terminal found."


TOOLS = {
    "whois": tool_whois,
    "ping": tool_ping,
    "file_check": tool_file_check,
    "shell": tool_shell,
    "launch": tool_launch,
}


def parse_and_run_tool(line):
    line = line.strip()
    if not line.upper().startswith("TOOL:"):
        return None
    parts = line[5:].strip().split(" ", 1)
    tool_name = parts[0].lower()
    arg = parts[1] if len(parts) > 1 else ""
    if tool_name in TOOLS:
        print(f"[TOOLS] Running tool: {tool_name}({arg})", flush=True)
        return TOOLS[tool_name](arg)
    return None


# ---- TTS ----
def speak_text(text):
    try:
        subprocess.run(
            [
                PIPER_EXE,
                "--model",
                PIPER_MODEL,
                "--config",
                PIPER_CONFIG,
                "--output_file",
                VOICE_OUTPUT,
            ],
            input=text.encode("utf-8"),
            check=True,
            stderr=subprocess.DEVNULL,
        )
        subprocess.run(["aplay", VOICE_OUTPUT], check=True, stderr=subprocess.DEVNULL)
    except Exception as e:
        print(f"[TOOLS] TTS Error: {e}", flush=True)


# ---- LLaMA query ----
def ask_llama(user_input):
    if llm is None:
        return "[TOOLS] Model not loaded. Check your model path."
    history_text = "\n".join(conversation_history)
    prompt = f"{SYSTEM_PROMPT}\n\n{history_text}\nAssistant:"
    try:
        resp = llm(
            prompt=prompt,
            max_tokens=150,
            stop=[
                "User:",
                "\nUser",
                "\n\n\n",
                "Note:",
                "Comment:",
                "Best regards",
                "```",
                "######",
            ],
        )
        raw = resp["choices"][0]["text"].strip()
        # Strip any padding junk the model appends
        raw = raw.split("######")[0].strip()
        return raw
    except Exception as e:
        return f"[TOOLS] LLaMA error: {e}"


# ---- Main input handler ----
def handle_input(user_input):
    conversation_history.append(f"User: {user_input}")
    answer = ask_llama(user_input)

    final_lines = []
    tool_results = []
    for line in answer.splitlines():
        result = parse_and_run_tool(line)
        if result:
            tool_results.append(result)
        else:
            final_lines.append(line)

    final_answer = "\n".join(final_lines).strip()
    if tool_results:
        final_answer += "\n" + "\n".join(tool_results)

    conversation_history.append(f"Assistant: {final_answer}")
    return final_answer


# ---- Terminal launchers ----
TERMINALS = [
    "mate-terminal",
    "x-terminal-emulator",
    "gnome-terminal",
    "xfce4-terminal",
    "lxterminal",
    "konsole",
    "xterm",
]


def launch_in_terminal(path):
    for term in TERMINALS:
        try:
            subprocess.Popen([term, "-e", path])
            return True
        except FileNotFoundError:
            continue
    return False


def open_terminal():
    for term in TERMINALS:
        try:
            subprocess.Popen([term])
            return True
        except FileNotFoundError:
            continue
    return False


# ---- MAIN LOOP ----
def main():
    print("--- CYON TOOLS ONLINE [LLAMA.CPP + VOICE] ---", flush=True)

    if not init_llama():
        print(
            "[TOOLS] WARNING: Running without LLaMA - slash commands still work.",
            flush=True,
        )

    while True:
        try:
            line = sys.stdin.readline()
        except KeyboardInterrupt:
            print("\n[TOOLS] Interrupted.", flush=True)
            break

        if not line:
            break

        user_input = line.strip()
        if not user_input:
            continue

        cmd = user_input.lower()

        if cmd == "/shutdown":
            print("Shutting down PC...", flush=True)
            os.system("shutdown -h now")
            break

        elif cmd == "/clear":
            conversation_history.clear()
            print("[TOOLS] Conversation history cleared.", flush=True)
            continue

        elif cmd == "/bye":
            print("[TOOLS] Shutting down Cyon Tools...", flush=True)
            break

        elif cmd == "/pyra":
            print("[TOOLS] Launching Pyra Tool...", flush=True)
            path = os.path.expanduser("~/cyon/pyra_tool/pyra_toolz")
            msg = (
                "[TOOLS] Pyra Tool launched."
                if launch_in_terminal(path)
                else "[TOOLS] No compatible terminal found."
            )
            print(msg, flush=True)

        elif cmd == "/cyon_cli":
            print("[TOOLS] Launching Cyon CLI...", flush=True)
            path = os.path.expanduser("~/cyon/bin/cyon_cli")
            msg = (
                "[TOOLS] Cyon CLI launched."
                if launch_in_terminal(path)
                else "[TOOLS] No compatible terminal found."
            )
            print(msg, flush=True)

        elif cmd == "/term":
            print("[TOOLS] Launching terminal...", flush=True)
            msg = (
                "[TOOLS] Terminal launched."
                if open_terminal()
                else "[TOOLS] No compatible terminal found."
            )
            print(msg, flush=True)

        elif cmd == "/status":
            status = "loaded" if llm is not None else "NOT LOADED"
            print(f"[TOOLS] Model status: {status}", flush=True)
            print(
                f"[TOOLS] History length: {len(conversation_history)} messages",
                flush=True,
            )

        else:
            answer = handle_input(user_input)
            print(f"CYON TOOLS: {answer}", flush=True)
            speak_text(answer)


if __name__ == "__main__":
    main()
