<!--
SPDX-License-Identifier: AGPL-3.0-or-later
Lireal Music - C++ audio visual rendering engine.
Copyright (C) 2026 Lireal contributors
-->

# 2026-06-17-release-nixos-real-gpu-list

- 新增 `.github/workflows/release.yml`，支持标签触发或手动触发可爱 Release。
- Release 构建矩阵覆盖 Kubuntu/Ubuntu 24.04、Arch Linux 与 NixOS。
- 新增 `flake.nix`，提供 `.#lirealMusic`、`.#lirealTui` 与开发 shell，增强 NixOS 全量支持。
- GUI 编码显卡列表改为真实枚举：
  - `nvidia-smi --query-gpu=index,name` 获取 NVIDIA CUDA 显卡；
  - `/dev/dri/renderD*` + `/sys/class/drm/.../vendor` 获取 VAAPI render 节点和厂商；
  - 选择 CUDA/VAAPI 设备时自动切换到对应编码后端。
- README 补充 NixOS、GitHub Release 与真实显卡列表说明。
