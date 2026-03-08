import sys
import subprocess
import os
import json
import urllib.request

# ---- CONFIG ----
OLLAMA_URL = "http://127.0.0.1:11434/api/generate"
OLLAMA_MODEL = "cyon"  # using custom Cyon modelfile

# Piper TTS paths
PIPER_EXE = "/home/cruxible/pyra_env/bin/piper"
PIPER_MODEL = "/home/cruxible/cyon/piper_models/en_US-joe-medium.onnx"
PIPER_CONFIG = "/home/cruxible/cyon/piper_models/en_US-joe-medium.onnx.json"
VOICE_OUTPUT = "/home/cruxible/cyon/piper_models/voice.wav"

# ---- Conversation history ----
conversation_history = []


# ---- Ollama query ----
def ask_ollama(prompt):
    try:
        payload = json.dumps({
            "model": OLLAMA_MODEL,
            "prompt": prompt,
            "stream": False
        }).encode("utf-8")
        req = urllib.request.Request(OLLAMA_URL, data=payload, headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=60) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            return data.get("response", "").strip()
    except Exception as e:
        return f"[LOCAL] Ollama error: {e}"


# ---- Main input handler ----
def handle_input(user_input):
    conversation_history.append(f"User: {user_input}")
    prompt = "\n".join(conversation_history) + "\nAssistant:"
    answer = ask_ollama(prompt)
    conversation_history.append(f"Assistant: {answer}")
    return answer


# ---- TTS ----
def speak_text(text):
    try:
        subprocess.run(
            [PIPER_EXE, "--model", PIPER_MODEL, "--config", PIPER_CONFIG, "--output_file", VOICE_OUTPUT],
            input=text.encode("utf-8"), check=True, stderr=subprocess.DEVNULL,
        )
        subprocess.run(["aplay", VOICE_OUTPUT], check=True, stderr=subprocess.DEVNULL)
    except Exception as e:
        print(f"[LOCAL] TTS Error: {e}", flush=True)


# ---- Terminal launchers ----
TERMINALS = ["mate-terminal", "x-terminal-emulator", "gnome-terminal",
             "xfce4-terminal", "lxterminal", "konsole", "xterm"]

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
    print("--- LOCAL CYON CORE ONLINE [VOICE ENABLED] ---", flush=True)

    while True:
        try:
            line = sys.stdin.readline()
        except KeyboardInterrupt:
            print("\n[LOCAL] Interrupted.", flush=True)
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
            print("[LOCAL] Conversation history cleared.", flush=True)
            continue

        elif cmd == "/bye":
            print("[LOCAL] Shutting down Cyon...", flush=True)
            break

        elif cmd == "/pyra":
            print("[LOCAL] Launching Pyra Tool...", flush=True)
            path = os.path.expanduser("~/cyon/pyra_tool/pyra_toolz")
            msg = "[LOCAL] Pyra Tool launched." if launch_in_terminal(path) else "[LOCAL] No compatible terminal found."
            print(msg, flush=True)

        elif cmd == "/cyon_cli":
            print("[LOCAL] Launching Cyon CLI...", flush=True)
            path = os.path.expanduser("~/cyon/bin/cyon_cli")
            msg = "[LOCAL] Cyon CLI launched." if launch_in_terminal(path) else "[LOCAL] No compatible terminal found."
            print(msg, flush=True)

        elif cmd == "/term":
            print("[LOCAL] Launching terminal...", flush=True)
            msg = "[LOCAL] Terminal launched." if open_terminal() else "[LOCAL] No compatible terminal found."
            print(msg, flush=True)

        elif cmd == "/status":
            print(f"[LOCAL] Ollama model: {OLLAMA_MODEL}", flush=True)
            print(f"[LOCAL] History length: {len(conversation_history)} messages", flush=True)

        else:
            answer = handle_input(user_input)
            print(f"CYON: {answer}", flush=True)
            speak_text(answer)


if __name__ == "__main__":
    main()
