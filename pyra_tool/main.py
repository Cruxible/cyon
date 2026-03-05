import platform
from pathlib import Path
import os
import time
import getpass
from rich.tree import Tree
from rich import print
from rich.console import Console
import sys

sys.path.append(str(Path.home() / "cyon" / "pyra_lib"))
sys.path.append(str(Path.home() / "cyon" / "pyra_tool"))
from pyra_shared import Input, main_logo, HonerableMentions
import subprocess
import random
import tarfile
import shutil
import json

if __name__ == "__main__":
    print(
        f" [white]System: {platform.system()}\n Node Name: {platform.node()}\n Release: {platform.release()}[/white]"
    )
    print(
        f" [white]Version: {platform.version()}\n Machine: {platform.machine()}\n Python version: {platform.python_version()}[/white]"
    )

    if platform.system() == "Linux":
        if Path("/sdcard/Download").exists():
            from pyra_toolz_termux import Main_Termux

            print(" Android directories exist.")
            Main_Termux.main()
        else:
            if not Path("/sdcard/Download").exists():
                from pyra_toolz import Main

                print(" Android directories do not exist...")
                pyra_toolz_dir = Path.home() / "cyon" / "pyra_tool"
                os.chdir(pyra_toolz_dir)
                with open("lotr_quotes.jsonl", "r", encoding="utf-8") as f:
                    lines = f.readlines()
                    random_line = random.choice(lines)
                    quote_data = json.loads(random_line)
                    print(
                        f' [red1]{quote_data["quote"]}"\n — {quote_data["character"]}[red1/]'
                    )
                Main.main()
