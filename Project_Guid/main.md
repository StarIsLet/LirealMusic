<aside>
🌸

**「莉瑞尔 Lireal」** 是一款运行在 Linux 平台上的高性能、全自动化音视频渲染引擎。本文档包含完整的 **项目计划书** 与 **技术实现书**。

</aside>

---

# 🌸 「莉瑞尔 Lireal」视听渲染引擎 · 项目计划书 (Project Plan)

## 1. 项目概述 (Project Overview)

「莉瑞尔 Lireal」是一款运行在 Linux 平台上的高性能、全自动化音视频渲染引擎。本项目旨在抛弃传统视频剪辑软件的繁琐与僵硬，通过底层矩阵运算与 AI 模型，将单张静态图片与普通音频转化为具有 **2.5D 深度视差**、**纯计算 3D 环绕声场** 及 **多声纹解构** 的高质量动态音乐视频。

## 2. 核心目标 (Core Objectives)

- **听觉降维打击：** 实现基于 Demucs 的高精度伴奏/人声分离，并结合 pyannote.audio 实现多人合唱的声纹识别。通过原生 numpy 矩阵运算，实现精准的左右声相调度与 3D 环绕重构。
- **视觉像素级微动：** 放弃传统死板的频谱圈，利用傅里叶变换 (FFT) 将特定频段（如重低音）精确映射到画面特定图层的呼吸感与视差位移上。
- **自动化生产力：** 全代码驱动，支持 `.lrc` 歌词自动打轴与画面合成，一键输出高质量 `.mp4` 文件。
- **极佳的用户体验：** 打造带有樱花粉与纯白配色的 PyQt 可视化控制台，支持毛玻璃高斯模糊效果，提供赏心悦目的交互体验。

## 3. 运行环境与硬件依赖 (Environment & Hardware)

| 类别 | 配置 | 说明 |
| --- | --- | --- |
| **操作系统** | Kubuntu Linux | KDE Plasma 桌面环境，保障高颜值 UI 融合 |
| **处理器** | i9-13980HX | 利用超多核心优势，执行音频解析与视频逐帧渲染的高并发任务 |
| **图形加速** | NVIDIA GeForce RTX 4080 Laptop GPU | CUDA 加速深度学习模型推理 |

## 4. 开发阶段规划 (Development Phases)

### Phase 1 · 核心音频引擎构筑 (Audio Core)

- [ ]  配置 Ubuntu 下的 PyTorch (CUDA 11/12) 深度学习环境。
- [ ]  集成 Demucs 实现音轨分离，封装处理接口。
- [ ]  集成 pyannote.audio 跑通多人声纹时间戳切片。
- [ ]  手写基于 numpy 的纯计算 3D 环绕（LFO）与空间声相映射算法。

### Phase 2 · 视觉视差与音画联动引擎 (Visual Core)

- [ ]  使用 rembg 实现静态图片的前景/背景自动化切分与深度映射。
- [ ]  编写傅里叶变换 (FFT) 音频频段解析器，提取高低频瞬态能量。
- [ ]  使用 OpenCV 与仿射变换算法，将频段能量映射为图层位移与缩放参数。

### Phase 3 · 渲染管线与歌词系统 (Render Pipeline)

- [ ]  集成 moviepy 编写逐帧渲染逻辑。
- [ ]  开发 `.lrc` 文件解析器，利用原生字体渲染器生成动态字幕层。
- [ ]  引入 Python 的 multiprocessing 模块，榨干 i9 处理器的多核性能进行并发渲染。

### Phase 4 · 「莉瑞尔」控制台开发 (UI/UX)

- [ ]  使用 PyQt6 / PySide6 搭建前端界面。
- [ ]  编写 QSS 样式表，严格采用 Sakura Pink & Pure White 调色板，设计圆角组件与流畅的加载进度条。
- [ ]  进行前后端逻辑绑定，实现参数可调的一键出片。

---

# 💻 「莉瑞尔 Lireal」技术实现书 (Implementation Document)

## 1. 系统架构设计

项目采用模块化设计，主程序 `lireal_core.py` 调度三个独立子引擎：

1. **AudioSpatioEngine** — 音频空间化引擎
2. **VisualParallaxEngine** — 视觉视差引擎
3. **LirealConsoleUI** — UI 控制台

## 2. 核心模块技术细节

### 2.1 音频空间化引擎 (AudioSpatioEngine)

- **多轨分离：** 调用 `demucs.api.Separator` 加载预训练模型，将源音频转换为包含 `drums`、`bass`、`other`、`vocals` 的张量矩阵。
- **声纹空间映射矩阵：** 放弃成品插件，使用纯数学方法计算声相。定义低频振荡器 (LFO)，将 pyannote.audio 识别出的人声切片，按时间戳 `t` 乘以动态权重，叠加至左右声道矩阵。

```python
import numpy as np

# 低频振荡器 (LFO) —— 纯计算 3D 环绕声相
def lfo_pan(t, freq=0.2, depth=0.8):
    # 返回左右声道增益，实现声场绕头旋转
    pan = depth * np.sin(2 * np.pi * freq * t)
    left_gain = np.clip(0.5 * (1 - pan), 0.0, 1.0)
    right_gain = np.clip(0.5 * (1 + pan), 0.0, 1.0)
    return left_gain, right_gain
```

### 2.2 视觉视差引擎 (VisualParallaxEngine)

- **频段联动响应：** 使用 `librosa.onset.onset_strength` 获取音频瞬态。当检测到瞬态能量峰值时，触发背景图层的 Gamma 值微调（亮度呼吸）与前景图层偏移。
- **图像变换：** 针对每一帧图像矩阵，计算其在时间轴 `t` 对应的音频振幅，应用变换矩阵实现 2.5D 错位平移。

### 2.3 视频帧合成与高并发渲染 (RenderPipeline)

- 采用 `moviepy.editor.VideoClip` 构建帧生成函数 `make_frame(t)`。
- **算力压榨方案：** 将视频总时长切割为 N 个片段（如 10 秒为一个 Chunk），利用 `concurrent.futures.ProcessPoolExecutor` 开启多进程，将多个 Chunk 分配给 i9-13980HX 的不同核心同时渲染，最后合并临时视频片段与处理好的无损 WAV 音频轨。

### 2.4 樱花粉控制台 (LirealConsoleUI)

- **UI 框架：** 基于 PyQt6。利用 KDE Plasma 系统的原生特性启用半透明窗体属性 `(Qt.WA_TranslucentBackground)`。
- **视觉代码示例（QSS 概念）：**

```css
QMainWindow {
    background-color: rgba(255, 255, 255, 0.85); /* 纯白亚克力底色 */
}
QProgressBar {
    border: 2px solid #FFD1DC; /* 樱花粉边框 */
    border-radius: 8px;
    background-color: #FFFFFF;
}
QProgressBar::chunk {
    background-color: #FFB7C5; /* 樱花粉进度 */
    border-radius: 6px;
}
```