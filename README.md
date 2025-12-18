# 🎨 TinySoftRenderer (C++ 软渲染器)

![C++](https://img.shields.io/badge/language-C%2B%2B17-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

> **A lightweight, programmable 3D software rasterizer written in C++ from scratch.**  
> 一个完全基于 CPU、不依赖图形 API (OpenGL/DirectX) 的 3D 软渲染引擎。

![Demo Screenshot](docs/demo.gif) 
*(⚠️请在这里替换一张你的泉此方渲染动图或截图，例如 screenshots/demo.png)*

## 📖 简介 (Introduction)

本项目是一个为了深入理解计算机图形学渲染管线（Graphics Pipeline）而编写的**软渲染器**。
核心逻辑（从顶点处理、光栅化到片段着色）完全由 C++ 手写实现，仅使用 OpenCV 进行最终的 FrameBuffer 显示和纹理读取。

项目重点实现了**二次元风格渲染（NPR / Toon Shading）**，包括卡通光照、脸部阴影优化。

This project is a software renderer built to understand the core mechanics of the graphics pipeline. It implements vertex processing, rasterization, and fragment shading purely in C++, utilizing OpenCV only for window display and image loading. The focus is on **Non-Photorealistic Rendering (NPR)** for anime characters.

## ✨ 核心特性 (Features)

### 📐 基础管线 (Pipeline)
*   **MVP 变换**: 完整的 Model-View-Projection 矩阵变换管线。
*   **光栅化 (Rasterization)**: 基于扫描线算法的三角形光栅化，支持透视校正插值。
*   **深度测试 (Z-Buffering)**: 解决物体前后遮挡关系。
*   **多重纹理支持 (Multi-Texturing)**: 支持解析 `.obj` + `.mtl`，自动识别并加载多张贴图。

### 🎨 着色与光照 (Shading & Lighting)
*   **Blinn-Phong 光照模型**: 支持环境光、漫反射和高光计算。
*   **卡通渲染 (Toon Shading)**: 
    *   基于阈值的色阶量化（Cel-Shading）。
    *   **脸部阴影优化**: 特殊处理脸部法线与光照，避免“脏阴影”。
*   **阴影映射 (Shadow Mapping)**: 
    *   两趟 Pass 渲染（Light Space Pass + Camera Space Pass）。
    *   支持 **PCF (Percentage-Closer Filtering)** 软阴影抗锯齿。

### 💧 高级效果 (Advanced)
*   **半透明混合 (Alpha Blending)**: 
    *   正确处理半透明材质（如眼镜、睫毛）。
    *   实现了渲染排序逻辑（先实体，后透明）与 Alpha Testing。
*   **自动缩放 (Auto-Scaling)**: 自动计算模型包围盒，将任意尺寸的模型居中并缩放到合适大小。

## 🛠️ 技术栈 (Tech Stack)

*   **语言**: C++ 17
*   **数学库**: [Eigen3](https://eigen.tuxfamily.org/) (用于向量与矩阵运算)
*   **显示/IO**: [OpenCV](https://opencv.org/) (用于窗口显示和图片读取)
*   **模型加载**: [tiny_obj_loader](https://github.com/tinyobjloader/tinyobjloader) (单头文件库)

## 🎮 操作说明 (Controls)

程序运行时支持第一人称漫游与模型交互：

| 按键 | 功能 |
| :--- | :--- |
| **W / S** | 相机 向上 / 向下 移动 |
| **A / D** | 相机 向左 / 向右 移动 |
| **Q / E** | 相机 向前 (推近) / 向后 (拉远) |
| **I / K** | 模型 绕 X 轴旋转 |
| **J / L** | 模型 绕 Y 轴旋转 |
| **ESC** | 退出程序 |

## 🚀 快速开始 (Build & Run)

### Windows (Visual Studio)
1.  确保已安装 **OpenCV** 和 **Eigen3**。
2.  使用 CMake 配置项目：
    ```bash
    mkdir build
    cd build
    cmake ..
    ```
3.  打开生成的 `.sln` 文件，编译运行。
4.  将 `.obj` 模型文件拖入控制台窗口，按回车即可加载。

### 文件结构
```text
├── main.cpp          # 主程序入口、交互逻辑、渲染循环
├── Renderer.h/cpp    # 渲染器核心（光栅化、着色器、Buffer管理）
├── MathUtils.h/cpp   # 数学工具库（矩阵生成、几何计算）
├── LoadModel.h/cpp   # 模型加载与材质处理
└── tiny_obj_loader.h # 第三方库
