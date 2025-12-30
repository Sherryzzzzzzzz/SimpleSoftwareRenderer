#pragma once
#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <opencv2/opencv.hpp>
#include <Eigen/Dense>

using namespace cv;
using namespace Eigen;

class Skybox {
public:
    Mat panorama;
    bool is_loaded = false;

    // 只加载一张全景图
    bool load(const std::string& path) {
        std::cout << "Loading Skybox: " << path << std::endl;
        panorama = imread(path);
        if (panorama.empty()) {
            std::cout << "Failed to load skybox!" << std::endl;
            is_loaded = false;
            return false;
        }
        is_loaded = true;
        return true;
    }

    // 球面投影采样
    Vector3i sample(Vector3f dir) {
        if (!is_loaded) return Vector3i(30, 30, 30);

        dir.normalize();
        const float PI = 3.14159265359f;

        // 1. 计算原始球坐标 UV
        float u = 0.5f + std::atan2(dir.z(), dir.x()) / (2.0f * PI);
        float v = 0.5f - std::asin(dir.y()) / PI;

        // =================================================
        // 🟢 核心修改：纹理缩放 (Tiling)
        // =================================================
        // scale = 1.0 : 原图大小
        // scale = 2.0 : 图片缩小一半 (重复 2 次)
        // scale = 3.0 : 图片缩小三分之一 (重复 3 次)
        float scale_u = 1.0f; // 水平方向重复次数 (根据喜好调)
        float scale_v = 1.0f; // 垂直方向重复次数

        u = u * scale_u;
        v = v * scale_v;

        // 🟢 处理重复 (Wrap / Repeat)
        // 当 u > 1.0 时，我们要取小数部分，让它从 0 开始重新画
        // 比如 1.5 -> 0.5
        u = u - std::floor(u);
        v = v - std::floor(v);

        // =================================================

        // 2. 采样
        int tx = (int)(u * (panorama.cols - 1));
        int ty = (int)(v * (panorama.rows - 1));

        tx = std::clamp(tx, 0, panorama.cols - 1);
        ty = std::clamp(ty, 0, panorama.rows - 1);

        Vec3b color = panorama.at<Vec3b>(ty, tx);
        return Vector3i(color[2], color[1], color[0]);
    }
};