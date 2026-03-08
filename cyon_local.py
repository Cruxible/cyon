import sys
import requests
import subprocess
import os

# ---- CONFIG ----
OLLAMA_URL = "http://127.0.0.1:11434/api/generate"
MODEL_NAME = "cyon"

# Piper paths
PIPER_EXE = "/home/cruxible/pyra_env/bin/piper"
PIPER_MODEL = "/home/cruxible/cyon/piper_models/en_US-joe-medium.onnx"
PIPER_CONFIG = "/home/cruxible/cyon/piper_models/en_US-joe-medium.onnx.json"
VOICE_OUTPUT = "/home/cruxible/cyon/piper_models/voice.wav"

# ---- Local conversation history ----
conversation_history = []


def speak_text(text):
    """Generates WAV via Piper and plays it immediately."""
    try:
        subprocess.run(
            [PIPER_EXE, "--model", PIPER_MODEL, "--config", PIPER_CONFIG, "--output_file", VOICE_OUTPUT],
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

        # Ignore slash commands — cyon_shell.py handles those
        if user_input.startswith("/"):
            continue

        # ---- AI query ----
        conversation_history.append(f"User: {user_input}")
        prompt = "\n".join(conversation_history)
        answer = ask_ollama(prompt)
        conversation_history.append(f"Assistant: {answer}")

        print(f"CYON: {answer}")
        sys.stdout.flush()
        speak_text(answer)


if __name__ == "__main__":
    main()
