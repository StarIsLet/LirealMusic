# 2026-06-21 手写 DSP 空间规划器

## 背景

用户希望不要只靠 FFmpeg 滤镜堆叠，而是加入更多手写纯算法实现，再交给 FFmpeg 执行，以获得更好的耳机 3D 沉浸感。

## 实现

新增 C++ 数据驱动空间规划器：

- `VirtualSpatialSource` 描述虚拟声源：方位角、距离、高度、增益、频段、宽度、反射属性。
- `makePanForSource` 根据声源位置计算双耳电平差 IID 和中心融合。
- `makeSpatialSourceFilter` 根据声源位置计算：
  - ITD 双耳时间差
  - 距离衰减
  - 高度空气感
  - 后方高频衰减
  - Haas 短延迟
  - 早期反射
  - 近场/远场宽度
- `makeHifiSurroundFilter` 使用 15 个虚拟声源构建耳机沉浸音场，然后由 FFmpeg 执行滤波图。

## 声场层

- main：保真主声
- center：中心人声
- sub：低频单声道锁定
- punch：低频冲击
- warm：中频温暖层
- presence：人声存在感
- sparkle：高频细节
- air：空气层
- leftnear/rightnear：近场左右
- leftfar/rightfar：远场左右
- front：前方舞台
- rear：后方反射
- ceiling：顶部空气

## 目标

比固定字符串滤镜更可控，方便之后把声源布局做成可调预设，同时保持视频模式只处理声音、不重绘画面。
