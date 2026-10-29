import asyncio
import json
import sys

import websockets


async def main(uri):
    async with websockets.connect(uri) as websocket:
        while True:
            message = json.loads(await websocket.recv())
            print(message["type"], message.get("channel", 0), json.loads(message["json"]))


if __name__ == "__main__":
    port = sys.argv[1] if len(sys.argv) > 1 else "8002"
    asyncio.run(main(f"ws://127.0.0.1:{port}"))
