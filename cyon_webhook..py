import requests
import json

WEBHOOK_URL = "webhook_url"


def send_message(content):
    data = {
        "content": content,
        "username": "Cyon",
        # "avatar_url": "https://i.imgur.com/4M34hi2.png",  # optional
    }

    response = requests.post(
        WEBHOOK_URL, data=json.dumps(data), headers={"Content-Type": "application/json"}
    )

    if response.status_code == 204:
        print("Message sent successfully.")
    else:
        print(f"Failed to send message: {response.status_code}")
        print(response.text)


if __name__ == "__main__":
    send_message("This is Cyon. I have been upgraded. This is all Cruxibles fault.")
