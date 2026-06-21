# lineart-auto-drawer

根据图像生成线稿，并根据线稿控制鼠标在其他程序中自动绘图的 C++ 工具。线稿提取达到 AI 级（Canny + Zhang-Suen 骨架细化 + potrace 贝塞尔矢量化），支持批量生成并导出为 SVG。

![Build](https://github.com/famoustao/lineart-auto-drawer/actions/workflows/build.yml/badge.svg)

> 当前同时构建 **Linux（Ubuntu 22.04 / 24.04）** 与 **Windows（MSVC 2022 x64）** 两个版本。

## 特性

- **AI 级线稿**：去噪 → 高斯模糊 → Canny 边缘 → Zhang-Suen 单像素骨架化 → potrace 三次贝塞尔矢量化
- **SVG 批量导出**：扫描目录中所有常见图片，自动生成同名 `.svg`
- **鼠标自动绘图**：根据矢量笔画顺序
  - **Linux**：X11 / XTest 扩展
  - **Windows**：`SendInput`（自动调用 `SetProcessDPIAware`，避免系统缩放导致坐标偏移）
- **两种矢量化**：默认用 libpotrace 得到平滑曲线；也可切换为内置折线近似
- **GitHub Actions 自动编译**：推送即编译，tag `v*` 自动打 Release 并发布两个平台的包

## 依赖

| 项目 | 说明 |
|---|---|
| CMake ≥ 3.16 | 构建系统 |
| C++17 编译器 | GCC / Clang / MSVC 2022 |
| OpenCV ≥ 4 | 图像读入 / 滤波 / Canny / 数学形态学 |
| libpotrace（可选） | 位图→平滑贝塞尔曲线 |
| X11 + XTest（Linux 可选）或 `windows.h`（Windows 可选） | 鼠标自动控制 |

## 本地构建

### Linux（Ubuntu 22.04 / 24.04）

```bash
sudo apt-get install build-essential cmake pkg-config \
    libopencv-dev libpotrace-dev libx11-dev libxtst-dev

git clone https://github.com/famoustao/lineart-auto-drawer.git
cd lineart-auto-drawer
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

./build/lineart_drawer --help
```

### Windows（MSVC 2022 x64，推荐 vcpkg 提供 OpenCV）

```powershell
# 1. 安装 vcpkg（如尚未安装）
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat -disableMetrics

# 2. 安装 OpenCV（动态库，构建最快）
C:\vcpkg\vcpkg install opencv:x64-windows --recurse

# 3. 配置并构建项目
cd lineart-auto-drawer
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_BUILD_TYPE=Release `
    -DVCPKG_TARGET_TRIPLET=x64-windows `
    -DCMAKE_TOOLCHAIN_FILE="C:\vcpkg\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Release -j

# 4. 运行（把 OpenCV 运行时 DLL 复制到 exe 同目录）
.\build\bin\Release\lineart_drawer.exe --help
```

> 不想用 vcpkg？也可下载 OpenCV 官方预编译包并设置 `-DOpenCV_DIR=<path>`。

## 使用

```bash
# 单张图片 → SVG（默认使用 potrace 平滑贝塞尔曲线；无 potrace 时自动回退内置折线）
./build/lineart_drawer single input.jpg output.svg

# 强制使用内置折线矢量化
./build/lineart_drawer single input.jpg output.svg use_potrace=0

# 批量处理目录
./build/lineart_drawer batch ./input_images ./output_svgs
./build/lineart_drawer batch ./input_images ./output_svgs canny_low=30 canny_high=90 export_png=1

# 根据线稿控制鼠标自动绘图（3 秒准备时间后开始描绘）
./build/lineart_drawer draw input.png offset_x=100 offset_y=100 scale=1.2 point_delay=5
```

**参数**

| 参数 | 默认 | 说明 |
|---|---|---|
| `canny_low` | 50 | Canny 低阈值 |
| `canny_high` | 150 | Canny 高阈值 |
| `adaptive` | 0 | 用自适应阈值替代 Canny（0/1） |
| `thin` | 1 | 是否使用 Zhang-Suen 细化 |
| `max_side` | 1024 | 内部缩放尺寸 |
| `denoise` | 7 | 去噪强度 |
| `stroke_width` | 1.0 | SVG 描边宽度 |
| `stroke_color` | `#000000` | SVG 描边颜色 |
| `use_potrace` | 1 | 是否使用 libpotrace |
| `export_png` | 0 | 是否同时导出 edges.png |
| `offset_x` / `offset_y` | 100 | 鼠标描图起点像素 |
| `scale` | 1.0 | 鼠标描图缩放 |
| `point_delay` / `stroke_delay` | 8 / 120 | 点/笔画间毫秒 |

## 目录结构

```
.
├── CMakeLists.txt
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
└── .github/workflows/build.yml   # GitHub Actions 自动编译（Linux + Windows）
```

## CI 说明

`.github/workflows/build.yml` 包含三个作业：

1. **build-linux**：`ubuntu-22.04` 与 `ubuntu-24.04` 上编译，产物上传到 Artifacts
2. **build-windows**：`windows-2022` 上用 vcpkg 装 OpenCV，编译 MSVC x64 版，产物含 `lineart_drawer.exe` + OpenCV 运行时 DLL
3. **release**：tag `v*` 时触发，把两个平台产物打包发布到 Release

实时状态：<https://github.com/famoustao/lineart-auto-drawer/actions>

手动触发 Release：

```bash
git tag v1.0.0
git push origin v1.0.0
```

然后 <https://github.com/famoustao/lineart-auto-drawer/releases> 会出现新版本，并附带：

- `lineart_drawer-linux.tar.gz`
- `lineart_drawer-windows.tar.gz`

## Windows 鼠标自动绘图提示

- Windows 版 `lineart_drawer draw` 会自动调用 `SetProcessDPIAware`，因此在 125% / 150% 缩放下，坐标仍以物理像素为单位，不会偏移。
- 鼠标操作走 `SendInput(INPUT_MOUSE)`，对绝大多数画图程序（Photoshop、Paint、Clip Studio、白板、网页）都有效。
- 运行前先把目标窗口置于前台，`offset_x / offset_y` 以屏幕左上角为原点（单位像素）。
