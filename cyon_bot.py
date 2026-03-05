import discord
import os
import requests
import asyncio
from concurrent.futures import ThreadPoolExecutor
import subprocess

# ---- CONFIG ----
TOKEN = "token"  # replace with your new token or use os.getenv("DISCORD_TOKEN")
AUTHORIZED_USER_ID = user_id

intents = discord.Intents.default()
intents.message_content = True
client = discord.Client(intents=intents)
executor = ThreadPoolExecutor()

# Conversation history stored per user
conversation_history = {}
MAX_HISTORY = 10  # max exchanges to keep (1 exchange = user message + reply)

# Piper paths
PIPER_MODEL_PATH = "/home/cruxible/cyon/piper_models/en_US-joe-medium.onnx"
PIPER_CONFIG_PATH = "/home/cruxible/cyon/piper_models/en_US-joe-medium.onnx.json"
VOICE_FILE = "/home/cruxible/cyon/piper_models/voice.wav"


def tts_piper_to_file(text, output_file=VOICE_FILE):
    env = os.environ.copy()
    env["DISPLAY"] = ""  # disable GUI attempt

    result = subprocess.run(
        [
            "piper",
            "--model",
            PIPER_MODEL_PATH,
            "--config",
            PIPER_CONFIG_PATH,
            "--output_file",
            output_file,
        ],
        input=text.encode(),
        capture_output=True,
        env=env,
    )
    print(f"[DEBUG] Piper return code: {result.returncode}")
    print(f"[DEBUG] Piper stderr: {result.stderr.decode()}")
    if result.returncode != 0:
        raise Exception(f"Piper failed with code {result.returncode}")
    return output_file


# ---- Llama 3 query ----
def ask_phi3(prompt, user_id):
    try:
        if user_id not in conversation_history:
            conversation_history[user_id] = []

        conversation_history[user_id].append(f"User: {prompt}")

        if len(conversation_history[user_id]) > MAX_HISTORY * 2:
            conversation_history[user_id] = conversation_history[user_id][
                -(MAX_HISTORY * 2) :
            ]

        full_prompt = "\n".join(conversation_history[user_id])
        response = requests.post(
            "http://localhost:11434/api/generate",
            json={"model": "cyon", "prompt": full_prompt, "stream": False},
            timeout=600,
        )
        reply = response.json()["response"]
        conversation_history[user_id].append(f"Assistant: {reply}")
        return reply
    except Exception as e:
        print(f"[DEBUG] Ollama error: {e}")
        return "Sorry, I had trouble thinking that one through. Try again!"


# ---- Send long message helper ----
async def send_long_message(channel, text):
    if len(text) <= 2000:
        await channel.send(text)
    else:
        lines = text.split("\n")
        chunk = ""
        for line in lines:
            if len(chunk) + len(line) + 1 > 2000:
                await channel.send(chunk)
                chunk = line + "\n"
            else:
                chunk += line + "\n"
        if chunk:
            await channel.send(chunk)


# ---- Read file attachment helper ----
async def read_attachment(attachment):
    try:
        file_bytes = await attachment.read()
        return file_bytes.decode("utf-8")
    except UnicodeDecodeError:
        try:
            return file_bytes.decode("latin-1")
        except Exception:
            return None


# ---- Events ----
@client.event
async def on_ready():
    print(f"[DEBUG] Logged in as {client.user}")


@client.event
async def on_message(message):
    print(
        f"[DEBUG] Message from {message.author} ({message.author.id}): {message.content}"
    )

    if message.author == client.user:
        return

    # ---- !ask (text only) ----
    if message.content.lower().startswith("/ask"):
        user_input = message.content[4:].strip()

        if message.attachments:
            attachment = message.attachments[0]
            if attachment.size > 1_000_000:
                await message.channel.send("File is too large (max 1MB for now).")
                return

            await message.channel.send(f"Reading `{attachment.filename}`...")
            file_content = await read_attachment(attachment)

            if file_content is None:
                await message.channel.send(
                    "Could not read that file — make sure it's a plain text file."
                )
                return

            if user_input:
                prompt = f"{user_input}\n\nFile contents:\n{file_content}"
            else:
                prompt = f"Please read the following file and summarize or respond to it:\n\n{file_content}"
        else:
            if not user_input:
                await message.channel.send(
                    "Please provide a prompt after `!ask`, or attach a file."
                )
                return
            prompt = user_input

        await message.channel.send("Thinking...")
        loop = asyncio.get_event_loop()
        response = await loop.run_in_executor(
            executor, ask_phi3, prompt, message.author.id
        )
        await send_long_message(message.channel, response)
        return

    # ---- !say (text + WAV) ----
    if message.content.lower().startswith("/say"):
        user_input = message.content[4:].strip()
        if not user_input:
            await message.channel.send(
                "Provide a prompt after `!say` for Cyon to speak."
            )
            return
        await message.channel.send("Thinking...")
        loop = asyncio.get_event_loop()
        response = await loop.run_in_executor(
            executor, ask_phi3, user_input, message.author.id
        )
        # Send text reply
        await send_long_message(message.channel, response)
        # Generate TTS WAV
        print(f"[DEBUG] Generating TTS for: {response[:50]}")
        try:
            await loop.run_in_executor(executor, tts_piper_to_file, response)
        except Exception as e:
            print(f"[DEBUG] Piper TTS failed: {e}")
            await message.channel.send("TTS generation failed.")
            return

        # Send WAV and delete file
        print(f"[DEBUG] WAV exists: {os.path.exists(VOICE_FILE)}")
        if os.path.exists(VOICE_FILE):
            print(f"[DEBUG] WAV file size: {os.path.getsize(VOICE_FILE)} bytes")
            try:
                await message.channel.send(file=discord.File(VOICE_FILE))
                print("[DEBUG] WAV sent successfully")
            except Exception as e:
                print(f"[DEBUG] Failed to send WAV: {e}")
            os.remove(VOICE_FILE)
        else:
            print("[DEBUG] WAV file not found after TTS!")
            await message.channel.send("Voice file missing after generation.")
        return

    # ---- !clear ----
    if message.content.lower().strip() == "/clear":
        conversation_history.pop(message.author.id, None)
        await message.channel.send("Conversation history cleared!")
        print(f"History after clear: {conversation_history}")
        return

    # ---- Admin-only commands ----
    if message.author.id != AUTHORIZED_USER_ID:
        return

    if message.content.lower().strip() == "/shutdown":
        await message.channel.send("Shutting down PC...")
        os.system("shutdown -h now")
    elif message.content.lower().strip() == "/bye":
        await message.channel.send("Shutting down Cyon...")
        await client.close()


client.run(TOKEN)
