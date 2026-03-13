from __future__ import annotations

import asyncio
from pathlib import Path

from doubao_client import DoubaoRealtimeBridge


TEST_QUERY = "请做一个简短自我介绍，并说明你是ESP32健康助手的语音后端。"


async def main() -> None:
    bridge = DoubaoRealtimeBridge()
    await bridge.connect()
    try:
        session_id = await bridge.start_text_session()
        print(f"Session started: {session_id}")

        output_path = Path("test_reply_audio.pcm")
        result = await bridge.collect_text_round(
            query=TEST_QUERY,
            output_audio_path=output_path,
        )

        print("ASR text:", result["asr_text"])
        print("Reply text:", result["reply_text"])
        print("Audio bytes:", result["audio_bytes"])
        print("Audio saved to:", output_path.resolve())
    finally:
        await bridge.finish_session()
        await bridge.close()


if __name__ == "__main__":
    asyncio.run(main())
