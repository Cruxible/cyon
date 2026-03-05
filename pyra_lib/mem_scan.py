from rich.console import Console
from rich.table import Table
from rich.live import Live
import psutil
import os
import time

console = Console()
current_process = psutil.Process(os.getpid())


def get_table():
    table = Table(title="Memory Monitor (like htop)")
    table.add_column("PID", justify="right")
    table.add_column("Name")
    table.add_column("RAM (MB)", justify="right")

    # Add self-monitoring at the TOP so it's always visible
    my_mem = current_process.memory_info().rss / 1024 / 1024
    my_name = current_process.name()
    table.add_row(
        str(current_process.pid),
        f"[bold yellow]{my_name} [self][/bold yellow]",
        f"[bold yellow]{my_mem:.2f}[/bold yellow]",
    )

    for proc in psutil.process_iter(["pid", "name", "memory_info"]):
        try:
            mem = proc.info["memory_info"].rss / 1024 / 1024
            if mem > 50:
                table.add_row(str(proc.info["pid"]), proc.info["name"], f"{mem:.2f}")
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            continue

    return table


with Live(console=console, refresh_per_second=1) as live:
    while True:
        live.update(get_table())
        time.sleep(1)
