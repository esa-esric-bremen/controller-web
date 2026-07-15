import asyncio
import websockets


async def test_websocket(uri):
    async with websockets.connect(uri) as websocket:
        while True:
            response = await websocket.recv()
            print(f"Received message: {response}")


if __name__ == "__main__":
    websocket_url = "ws://localhost:8000"
    asyncio.run(test_websocket(websocket_url))