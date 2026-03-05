import sys
import requests
import subprocess
import os


# ---- CONFIG ----
OLLAMA_URL = "http://localhost:11434/api/generate"
MODEL_NAME = "cyon"

# Piper paths
PIPER_EXE = "/home/cruxible/pyra_env/bin/piper"
PIPER_MODEL = "/home/cruxible/cyon/piper_models/en_US-joe-medium.onnx"
PIPER_CONFIG = "/home/cruxible/cyon/piper_models/en_US-joe-medium.onnx.json"
VOICE_OUTPUT = "/home/cruxible/cyon/piper_models/voice.wav"

# ---- Local conversation history ----
conversation_history = []  # stores user + assistant messages for this session


def speak_text(text):
    """Generates WAV via Piper and plays it immediately."""
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
        print(f"DEBUG: TTS Error - {e}")
        sys.stdout.flush()


def ask_ollama(prompt):
    try:
        response = requests.post(
            OLLAMA_URL,
            json={"model": MODEL_NAME, "prompt": prompt, "stream": False},
            timeout=60,
        )
        return response.json().get("response", "Error: No response.")
    except Exception as e:
        return f"Ollama Error: {e}"


def main():
    print("--- LOCAL CYON CORE ONLINE [VOICE ENABLED] ---")
    sys.stdout.flush()

    while True:
        line = sys.stdin.readline()
        if not line:
            break

        user_input = line.strip()
        if not user_input:
            continue

        # ---- Admin/local commands ----
        if user_input.lower() == "/shutdown":
            print("Shutting down PC...")
            sys.stdout.flush()
            os.system("shutdown -h now")
            break

        elif user_input.lower() == "/clear":
            conversation_history.clear()
            print("Conversation history cleared!")
            sys.stdout.flush()
            continue

        elif user_input.lower() == "/bye":
            print("Shutting down Cyon...")
            sys.stdout.flush()
            break

        elif user_input.lower() == "/pyra":
            print("[LOCAL] ⚠ Launching Pyra Tool...")
            sys.stdout.flush()

            terminals = [
                "mate-terminal",
                "x-terminal-emulator",
                "gnome-terminal",
                "xfce4-terminal",
                "lxterminal",
                "konsole",
                "xterm",
            ]

            pyra_path = os.path.expanduser("~/cyon/pyra_tool/pyra_toolz")
            opened = False
            for term in terminals:
                try:
                    subprocess.Popen([term, "-e", pyra_path])
                    opened = True
                    break
                except FileNotFoundError:
                    continue

            if opened:
                print("[LOCAL] Pyra Tool launched in a new terminal.")
            else:
                print("[LOCAL] ⚠ No compatible terminal found to run Pyra Tool.")

        elif user_input.lower() == "/cyon_cli":
            print("[LOCAL] ⚠ Launching Cyon CLI...")
            sys.stdout.flush()

            terminals = [
                "mate-terminal",
                "x-terminal-emulator",
                "gnome-terminal",
                "xfce4-terminal",
                "lxterminal",
                "konsole",
                "xterm",
            ]

            pyra_path = os.path.expanduser("~/cyon/bin/cyon_cli")
            opened = False
            for term in terminals:
                try:
                    subprocess.Popen([term, "-e", pyra_path])
                    opened = True
                    break
                except FileNotFoundError:
                    continue

            if opened:
                print("[LOCAL] Cyon CLI launched in a new terminal.")
            else:
                print("[LOCAL] ⚠ No compatible terminal found to run Cyon CLI.")

        elif user_input.lower() == "/term":
            print("[LOCAL] ⚠ Launching system terminal...")
            sys.stdout.flush()

            terminals = [
                "mate-terminal",
                "x-terminal-emulator",
                "gnome-terminal",
                "xfce4-terminal",
                "lxterminal",
                "konsole",
                "xterm",
            ]

            opened = False
            for term in terminals:
                try:
                    # Launch terminal without any command so it stays interactive
                    subprocess.Popen([term])
                    opened = True
                    break
                except FileNotFoundError:
                    continue

            if opened:
                print("[LOCAL] Terminal launched.")
            else:
                print("[LOCAL] ⚠ No compatible terminal found.")

        # ---- Normal AI query ----
        conversation_history.append(f"User: {user_input}")
        prompt = "\n".join(conversation_history)
        answer = ask_ollama(prompt)
        conversation_history.append(f"Assistant: {answer}")

        print(f"CYON: {answer}")
        sys.stdout.flush()
        speak_text(answer)


if __name__ == "__main__":
    main()
