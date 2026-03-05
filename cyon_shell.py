import sys
import os
import subprocess

# ---- cyon_shell.py ----
# Dedicated command handler for main_cyon.
# Handles echo and slash commands independently of Ollama or the bot.

TERMINALS = [
    "mate-terminal",
    "x-terminal-emulator",
    "gnome-terminal",
    "xfce4-terminal",
    "lxterminal",
    "konsole",
    "xterm",
]


def launch_in_terminal(cmd_path=None):
    for term in TERMINALS:
        try:
            if cmd_path:
                subprocess.Popen([term, "-e", cmd_path])
            else:
                subprocess.Popen([term])
            return True
        except FileNotFoundError:
            continue
    return False


def handle_command(user_input):
    cmd = user_input.lower().strip()

    if cmd == "/shutdown":
        print("SHELL: Shutting down PC...")
        sys.stdout.flush()
        os.system("shutdown -h now")

    elif cmd == "/bye":
        print("SHELL: Closing...")
        sys.stdout.flush()
        sys.exit(0)

    elif cmd == "/clear":
        print("SHELL: Clear requested.")
        sys.stdout.flush()

    elif cmd == "/pyra":
        pyra_path = os.path.expanduser("~/cyon/pyra_tool/pyra_toolz")
        if launch_in_terminal(pyra_path):
            print("SHELL: Pyra Tool launched.")
        else:
            print("SHELL: No compatible terminal found for Pyra Tool.")
        sys.stdout.flush()

    elif cmd == "/cyon_cli":
        cli_path = os.path.expanduser("~/cyon/bin/cyon_cli")
        if launch_in_terminal(cli_path):
            print("SHELL: Cyon CLI launched.")
        else:
            print("SHELL: No compatible terminal found for Cyon CLI.")
        sys.stdout.flush()

    elif cmd == "/term":
        if launch_in_terminal():
            print("SHELL: Terminal launched.")
        else:
            print("SHELL: No compatible terminal found.")
        sys.stdout.flush()

    else:
        # Unknown command — just echo it back, bot will handle it
        print(f"SHELL: Unknown command: {user_input}")
        sys.stdout.flush()


def main():
    print("--- CYON SHELL ONLINE ---")
    sys.stdout.flush()

    while True:
        line = sys.stdin.readline()
        if not line:
            break

        user_input = line.strip()
        if not user_input:
            continue

        if user_input.startswith("/"):
            handle_command(user_input)
        else:
            # Echo non-command input back to the log
            print(f"You: {user_input}")
            sys.stdout.flush()


if __name__ == "__main__":
    main()
