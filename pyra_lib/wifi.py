import subprocess
import re


def scan_wifi():
    try:
        # Run the iwlist scan command
        result = subprocess.check_output(
            ["sudo", "iwlist", "scan"], stderr=subprocess.DEVNULL
        ).decode("utf-8")

        # Parse the output
        cells = result.split("Cell")
        networks = []
        for cell in cells[1:]:
            ssid_search = re.search(r'ESSID:"(.+?)"', cell)
            quality_search = re.search(r"Quality=(\d+)/(\d+)", cell)
            if ssid_search and quality_search:
                ssid = ssid_search.group(1)
                quality = (
                    int(quality_search.group(1)) * 100 // int(quality_search.group(2))
                )
                networks.append((ssid, quality))

        # Print networks
        print("Available Wi-Fi Networks:")
        for ssid, quality in networks:
            print(f"{ssid}: {quality}% signal strength")

    except subprocess.CalledProcessError:
        print("Error: Make sure you have Wi-Fi and run this script with sudo.")


if __name__ == "__main__":
    scan_wifi()
