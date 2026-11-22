# 音效来源与制作说明

## 中文提示语音

`eatChess.wav`、`generalSound.wav` 和 `WinSound.wav` 分别用于“吃！”、“将军！”和“胜利！”提示，均为本项目离线生成的 AI 合成语音。使用 Qwen3-TTS 的通用预设音色 `uncle_fu`，生成日期为 2026-11-17。

生成模型采用 Apache-2.0 许可，推理工具采用 MIT 许可。项目运行时通过 Qt 播放已经生成的 WAV 文件。

- 原始模型与代码见 [Qwen3-TTS](https://github.com/QwenLM/Qwen3-TTS) 和 [1.7B CustomVoice 模型](https://huggingface.co/Qwen/Qwen3-TTS-12Hz-1.7B-CustomVoice)。
- 本次使用的量化权重见 [Serveurperso/Qwen3-TTS-GGUF](https://huggingface.co/Serveurperso/Qwen3-TTS-GGUF)。
- 本次使用的 C++ 推理工具见 [qwentts.cpp](https://github.com/ServeurpersoCom/qwentts.cpp)，提交号为 `2edb35f`。

使用 `qwen-talker-1.7b-customvoice-Q4_K_M.gguf` 和 `qwen-tokenizer-12hz-Q8_0.gguf`，语言参数为 `Chinese`，随机种子为 `20261117`，最多生成 36 个音频帧，其他采样参数使用工具默认值。风格指令如下。

```text
Speak in Mandarin Chinese as a powerful traditional Chinese chess game announcer. Shout loudly with a strong chest voice, forceful attack, confident martial energy, short and decisive. Clear pronunciation, no extra words, no laughter.
```

原始输出为 24 kHz 单声道 PCM16。后处理使用 75 Hz 高通、6 ms 淡入、15 ms 淡出，重采样为 44.1 kHz 单声道 PCM16，并将峰值归一化至约 0.85 满幅。

“胜利！”使用同一音色，随机种子为 `20261118`，风格指令为“用浑厚有力的男声，短促、果断、大声地喊出胜利的喜悦，像传统象棋游戏中的获胜提示。”。

模型文件的 SHA-256 校验值如下。

```text
qwen-talker-1.7b-customvoice-Q4_K_M.gguf
cc328834a631bc08bf9f43e62fa23f8a1383d9b429864ce6690cfb172077fc4a

qwen-tokenizer-12hz-Q8_0.gguf
1883beeed99348fc35e23dd225e9082f93f6f8c109330a33d935baa8acdbfd94
```

## 棋盘与操作音

`selectChess.wav`、`moveChess.wav`、`backChess.wav` 和 `drawSound.wav` 由 `resources/sound/generate_board_sounds.py` 使用正弦振荡、衰减包络和确定性噪声合成。该脚本及其生成的这四个 WAV 文件采用 [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)。

## 分发位置

Windows 发行目录随 `bin/resources/licenses/AUDIO_ATTRIBUTION.md` 提供本说明。项目代码的许可见项目原有许可证文件。
