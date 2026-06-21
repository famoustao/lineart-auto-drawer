# lineart-auto-drawer

根据图像生成线稿，并根据线稿控制鼠标在其他程序中自动绘图的 C++ 工具。线稿提取达到 AI 级（Canny + Zhang-Suen 骨架细化 + potrace 贝塞尔矢量化），支持批量生成并导出为 SVG。

![Build](https://github.com/auto-dev-user/lineart-auto-drawer/actions/workflows/build.yml/badge.svg)

## 特性

- **AI 级线稿**：去噪 → 高斯模糊 → Canny 边缘 → Zhang-Suen 单像素骨架化 → potrace 三次贝塞尔矢量化
- **SVG 批量导出**：扫描目录中所有常见图片，自动生成同名 `.svg`
- **鼠标自动绘图**：根据矢量笔画顺序，在 Linux（X11/XTest）或 Windows（SendInput）下自动移动并按下左键描绘
- **两种矢量化**：优先使用 libpotrace 得到平滑曲线；也可切换为内置折线近似
- **GitHub Actions 自动编译**：推送代码后自动在 Ubuntu 22.04 / 24.04 编译、并对 tag `v*` 自动打 Release

## 依赖

| 项目 | 说明 |
|---|---|
| CMake ≥ 3.16 | 构建系统 |
| C++17 编译器 | GCC / MSVC |
| OpenCV ≥ 4 | `libopencv-dev` |
| libpotrace | 位图→平滑贝塞尔曲线（可选，缺省自动回退内置矢量化） |
| X11 + XTest（Linux）或 `windows.h`（Windows） | 鼠标自动控制 |

## 本地构建

```bash
# Ubuntu / Debian
sudo apt-get install build-essential cmake pkg-config \
    libopencv-dev libpotrace-dev libx11-dev libxtst-dev

git clone https://github.com/auto-dev-user/lineart-auto-drawer.git
cd lineart-auto-drawer
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# 输出: build/lineart_drawer
```

Windows:
```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_WINAPI=ON
cmake --build build --config Release
```

## 使用

```bash
# 单张图片 → SVG（默认使用 potrace 平滑贝塞尔曲线）
./build/lineart_drawer single input.jpg output.svg

# 单张图片 → SVG（内置折线矢量化，不依赖 potrace）
./build/lineart_drawer single input.jpg output.svg use_potrace=0

# 批量处理目录
./build/lineart_drawer batch ./input_images ./output_svgs
./build/lineart_drawer batch ./input_images ./output_svgs canny_low=30 canny_high=90 export_png=1

# 根据线稿自动控制鼠标绘图（3 秒准备时间后开始描绘）
./build/lineart_drawer draw input.png offset_x=100 offset_y=100 scale=1.2 point_delay=5
```

**参数**

| 参数 | 默认 | 说明 |
|---|---|---|
| `canny_low` | 50 | Canny 低阈值 |
| `canny_high` | 150 | Canny 高阈值 |
| `adaptive` | 0 | 使用自适应阈值替代 Canny（0/1） |
| `thin` | 1 | 是否使用 Zhang-Suen 细化 |
| `max_side` | 1024 | 内部缩放尺寸 |
| `denoise` | 7 | 去噪强度 |
| `stroke_width` | 1.0 | SVG 描边宽度 |
| `stroke_color` | `#000000` | SVG 描边颜色 |
| `use_potrace` | 1 | 是否使用 libpotrace（需安装） |
| `export_png` | 0 | 是否同时导出 edges.png |
| `offset_x` / `offset_y` | 100 | 鼠标描图起点像素 |
| `scale` | 1.0 | 鼠标描图缩放 |
| `point_delay` / `stroke_delay` | 8 / 120 | 点/笔画间毫秒 |

## 目录结构

```
.
├── CMakeLists.txt
├── CMakePresets.json
├── include/
│   ├── common.h
│   ├── image_preprocessor.h
│   ├── line_extractor.h
│   ├── svg_exporter.h
│   ├── mouse_controller.h
│   └── batch_processor.h
├── src/
│   ├── main.cpp
│   ├── image_preprocessor.cpp
│   ├── line_extractor.cpp
│   ├── svg_exporter.cpp
│   ├── mouse_controller.cpp
│   └── batch_processor.cpp
└── .github/workflows/build.yml   # GitHub Actions 自动编译
```

## CI 说明

`.github/workflows/build.yml` 包含两个作业：

- **build-linux**：在 `ubuntu-22.04` 与 `ubuntu-24.04` 上编译，产物上传到 Artifacts
- **release**：当 tag 为 `v*`（例如 `git tag v1.0.0 && git push origin v1.0.0`）时，自动打包产物并创建 GitHub Release

本地可以直接观察 Actions 状态：
`https://github.com/<你的用户名>/lineart-auto-drawer/actions`
