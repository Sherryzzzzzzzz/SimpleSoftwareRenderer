#include "Renderer.h"
#include "MathUtils.h" 
#include <algorithm> 
#include <cmath> 
#include <limits> // 用于 infinity

using namespace MathUtils;

// --- 构造函数 ---
Renderer::Renderer(int w, int h) : width(w), height(h) {
    frame_buffer = Mat(height, width, CV_8UC3);
    z_buffer.resize(width * height);
    // 初始化 z_buffer 为无穷大
    std::fill(z_buffer.begin(), z_buffer.end(), std::numeric_limits<float>::infinity());
}

Renderer::~Renderer() {
}

// --- 清屏 ---
void Renderer::clear() {
    frame_buffer.setTo(Scalar(0, 0, 0));
    // 清空 Z-Buffer
    std::fill(z_buffer.begin(), z_buffer.end(), std::numeric_limits<float>::infinity());
}

Mat& Renderer::get_frame_buffer() {
    return frame_buffer;
}

// --- 画点 ---
void Renderer::set_pixel(int x, int y, const Vector3i& color) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    int cv_y = height - 1 - y;
    int cv_x = x;
    frame_buffer.at<Vec3b>(cv_y, cv_x) = Vec3b(color.z(), color.y(), color.x());
}

// --- 画线 ---
void Renderer::draw_line(Vector2i p0, Vector2i p1, Vector3i color) {
    bool steep = false;
    int x0 = p0.x(), y0 = p0.y();
    int x1 = p1.x(), y1 = p1.y();

    if (std::abs(x1 - x0) < std::abs(y1 - y0)) {
        steep = true;
        std::swap(x0, y0);
        std::swap(x1, y1);
    }
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    int dx = x1 - x0;
    int dy = y1 - y0;
    int derror = std::abs(dy) * 2;
    int error = 0;
    int y_step = (y1 > y0 ? 1 : -1);
    int y = y0;

    for (int x = x0; x <= x1; x++) {
        if (steep) set_pixel(y, x, color);
        else       set_pixel(x, y, color);

        error += derror;
        if (error > dx) {
            y += y_step;
            error -= dx * 2;
        }
    }
}

// --- 光栅化三角形---
void Renderer::rasterize_triangle(Vector3f v0, Vector3f v1, Vector3f v2, Vector3i color) {
    // 1. 包围盒 (取整)
    int min_x = (int)std::min({ v0.x(), v1.x(), v2.x() });
    int max_x = (int)std::max({ v0.x(), v1.x(), v2.x() });
    int min_y = (int)std::min({ v0.y(), v1.y(), v2.y() });
    int max_y = (int)std::max({ v0.y(), v1.y(), v2.y() });

    // 【关键修正】把 3D 顶点降维成 2D 用于计算重心坐标
    Vector2f v0_2d = v0.head<2>();
    Vector2f v1_2d = v1.head<2>();
    Vector2f v2_2d = v2.head<2>();

    for (int x = min_x; x <= max_x; x++) {
        for (int y = min_y; y <= max_y; y++) {

            // 2. 计算重心坐标 (使用降维后的 2D 向量)
            auto [alpha, beta, gamma] = compute_barycentric((float)x + 0.5f, (float)y + 0.5f, v0_2d, v1_2d, v2_2d);

            // 3. 检查是否在三角形内
            if (alpha >= 0 && beta >= 0 && gamma >= 0) {

                // 4. 插值 Z 值
                float z_current = alpha * v0.z() + beta * v1.z() + gamma * v2.z();

                // 5. 深度测试
                int index = y * width + x;
                if (z_current < z_buffer[index]) {
                    z_buffer[index] = z_current;
                    set_pixel(x, y, color);
                }


            }
        }
    }
}

// --- 彩色三角形测试 ---
void Renderer::rasterize_triangle_test(Vector2i v0, Vector2i v1, Vector2i v2) {
    Vector3f c0(255.0f, 0.0f, 0.0f);
    Vector3f c1(0.0f, 255.0f, 0.0f);
    Vector3f c2(0.0f, 0.0f, 255.0f);

    int min_x = std::min({ v0.x(), v1.x(), v2.x() });
    int max_x = std::max({ v0.x(), v1.x(), v2.x() });
    int min_y = std::min({ v0.y(), v1.y(), v2.y() });
    int max_y = std::max({ v0.y(), v1.y(), v2.y() });

    // 转换成 float 向量用于计算
    Vector2f v0f = v0.cast<float>();
    Vector2f v1f = v1.cast<float>();
    Vector2f v2f = v2.cast<float>();

    for (int x = min_x; x <= max_x; x++) {
        for (int y = min_y; y <= max_y; y++) {
            auto [alpha, beta, gamma] = MathUtils::compute_barycentric(x + 0.5f, y + 0.5f, v0f, v1f, v2f);

            if (alpha >= 0 && beta >= 0 && gamma >= 0) {
                Vector3f color = alpha * c0 + beta * c1 + gamma * c2;
                set_pixel(x, y, Vector3i((int)color.x(), (int)color.y(), (int)color.z()));
            }
        }
    }
}

void Renderer::rasterize_triangle(Vector3f v0, Vector3f v1, Vector3f v2,
    Vector2f uv0, Vector2f uv1, Vector2f uv2,
    Vector3f n0, Vector3f n1, Vector3f n2,
    Vector4f s0, Vector4f s1, Vector4f s2,
    const cv::Mat& texture, bool is_face, float alpha) {

    // 1. 包围盒计算 (保持不变)
    int min_x = (int)std::min({ v0.x(), v1.x(), v2.x() });
    int max_x = (int)std::max({ v0.x(), v1.x(), v2.x() });
    int min_y = (int)std::min({ v0.y(), v1.y(), v2.y() });
    int max_y = (int)std::max({ v0.y(), v1.y(), v2.y() });

    min_x = std::max(0, min_x); max_x = std::min(width - 1, max_x);
    min_y = std::max(0, min_y); max_y = std::min(height - 1, max_y);

    // 2D 顶点用于计算重心坐标
    Vector2f v0_2d = v0.head<2>();
    Vector2f v1_2d = v1.head<2>();
    Vector2f v2_2d = v2.head<2>();

    for (int x = min_x; x <= max_x; x++) {
        for (int y = min_y; y <= max_y; y++) {

            // 2. 计算重心坐标
            auto [alpha, beta, gamma] = MathUtils::compute_barycentric((float)x + 0.5f, (float)y + 0.5f, v0_2d, v1_2d, v2_2d);

            if (alpha >= 0 && beta >= 0 && gamma >= 0) {

                // 3. 深度测试 (保持不变)
                float z_current = alpha * v0.z() + beta * v1.z() + gamma * v2.z();
                int index = y * width + x;

                if (z_current < z_buffer[index]) {
                    z_buffer[index] = z_current;

                float shadow_factor = 1.0f; // 1.0 = 全亮，0.5 = 阴影

                // 1. 插值算出当前像素在“光空间”的坐标
                Vector4f s_interpolated = alpha * s0 + beta * s1 + gamma * s2;

                // 2. 透视除法 (归一化到 [-1, 1] 的 NDC 空间)
                // 这一步千万不能忘！
                Vector3f s_ndc = s_interpolated.head<3>() / s_interpolated.w();

                // 3. 映射到 [0, 1] 的 UV 空间 (因为 ShadowMap 的坐标是 0~1)
                float u = s_ndc.x() * 0.5f + 0.5f;
                float v = s_ndc.y() * 0.5f + 0.5f;
                float current_depth = s_ndc.z(); // 当前点距离光的深度 ([-1, 1] 之间)

                // 4. 边界检查 (防止去查 ShadowMap 外面的值)
                if (u >= 0 && u < 1 && v >= 0 && v < 1) {
                    // 5. 算出在 ShadowMap 里的整数坐标
                    int shadow_x = (int)(u * (shadow_width - 1));
                    int shadow_y = (int)(v * (shadow_height - 1));
                    int shadow_idx = shadow_y * shadow_width + shadow_x;

                    // 6. 查表：光能看到的最近深度是多少？
                    float closest_depth = shadow_buffer[shadow_idx];

                    // 7. 比较：我比最近深度还要远吗？
                    if (current_depth - 0.005f > closest_depth) {
                        shadow_factor = 0.5f; // 在阴影里！变暗
                    }
                }

                shadow_factor = 1.0f; // 最终的遮挡因子

                // 🟢 === PCF (Percentage-Closer Filtering) 开始 ===

                // 定义 filter 大小 (3x3)
                int filter_size = 1; // 1 表示左右各扩 1 格，即 -1, 0, 1
                float visibility = 0.0f;
                float total_samples = 0.0f;

                // 基础坐标 (映射到 map 尺寸)
                float base_u = u * (shadow_width - 1);
                float base_v = v * (shadow_height - 1);

                // 循环采样周围 9 个点
                for (int x = -filter_size; x <= filter_size; x++) {
                    for (int y = -filter_size; y <= filter_size; y++) {

                        // 算出邻居的整数坐标
                        int neighbor_x = (int)(base_u + x);
                        int neighbor_y = (int)(base_v + y);

                        // 边界检查 (防止数组越界崩溃)
                        if (neighbor_x >= 0 && neighbor_x < shadow_width &&
                            neighbor_y >= 0 && neighbor_y < shadow_height) {

                            int idx = neighbor_y * shadow_width + neighbor_x;
                            float closest_depth = shadow_buffer[idx];

                            // 比较 (带 Bias)
                            // 如果没有被挡住，记为 1.0，被挡住记为 0.0
                            if (current_depth - 0.02f < closest_depth) {
                                visibility += 1.0f;
                            }
                        }
                        total_samples += 1.0f;
                    }
                }

                shadow_factor = visibility / total_samples;

                // 脸部强制不接受阴影
                if (is_face) shadow_factor = 1.0f;

                Vector3f normal = (alpha * n0 + beta * n1 + gamma * n2).normalized();

                Vector3f light_dir(1.0f, 1.0f, 1.0f);

				float intensity = std::max(0.0f, normal .dot(light_dir));

                float level;
                if (intensity > 0.5f) {
                    level = 1.0f; // 亮部：全亮
                }
                else if (intensity > 0.1f) {
                    level = 0.6f; // (可选) 增加一个中间过渡层，让衣服褶皱稍微柔和点
                }
                else {
                    level = 0.4f; // 暗部：给个基础亮度，不要死黑
                }

                if (is_face)
                {
					level = 1.0f;
                }

                float ambient = 0.1f;
                // === 🟢 4. 核心：UV 插值 ===
                // 算出当前像素在图片上的相对位置 (0.0 ~ 1.0)
                u = alpha * uv0.x() + beta * uv1.x() + gamma * uv2.x();
                v = alpha * uv0.y() + beta * uv1.y() + gamma * uv2.y();

                // === 🟢 5. 纹理采样 (查颜色) ===
                // A. 限制范围在 0~1 之间 (防止越界)
                u = std::min(1.0f, std::max(0.0f, u));
                v = std::min(1.0f, std::max(0.0f, v));

                // B. 映射到图片像素坐标
                int tex_x = (int)(u * (texture.cols - 1));
                int tex_y = (int)((1.0f - v) * (texture.rows - 1));

                // C. 读取颜色 (OpenCV 默认是 BGR)
                Vec3b color_bgr = texture.at<Vec3b>(tex_y, tex_x);
                float final_shadow = 0.5f + (shadow_factor * 0.5f);
                
                Vector3f final_color;
                float light_strength = level * shadow_factor * final_shadow;
                final_color.x() = color_bgr[2] * light_strength;
                final_color.y() = color_bgr[1] * light_strength;
                final_color.z() = color_bgr[0] * light_strength;

				if (!is_face)
				{
					// === 🟢 高光 (Specular) ===
                    Vector3f view_dir(0, 0, 1);
                   
                    Vector3f h = (light_dir + view_dir).normalized();
                    float spec = std::pow(std::max<float>(0.0f, normal.dot(h)), 128);
                    final_color = final_color + Vector3f(255, 255, 255) * spec;

                    
				}   

                // 防止过曝 (>255)
                final_color.x() = std::min(255.0f, final_color.x());
                final_color.y() = std::min(255.0f, final_color.y());
                final_color.z() = std::min(255.0f, final_color.z());

                set_pixel(x, y, final_color.cast<int>());
                }
            }
        }
    }
}

// 初始化
void Renderer::init_shadow_buffer(int w, int h) {
    shadow_width = w;
    shadow_height = h;
    shadow_buffer.resize(w * h);
    // 初始化为无穷大 (表示还没东西)
    std::fill(shadow_buffer.begin(), shadow_buffer.end(), std::numeric_limits<float>::max());
}

// 影子光栅化 (极简版)
void Renderer::rasterize_shadow(Vector3f v0, Vector3f v1, Vector3f v2) {
    // 1. 包围盒 (针对 shadow map 的大小)
    int min_x = (int)std::min({ v0.x(), v1.x(), v2.x() });
    int max_x = (int)std::max({ v0.x(), v1.x(), v2.x() });
    int min_y = (int)std::min({ v0.y(), v1.y(), v2.y() });
    int max_y = (int)std::max({ v0.y(), v1.y(), v2.y() });

    min_x = std::max(0, min_x); max_x = std::min(shadow_width - 1, max_x);
    min_y = std::max(0, min_y); max_y = std::min(shadow_height - 1, max_y);

    // 降维用于计算重心 (3D -> 2D)
    Vector2f t0 = v0.head<2>();
    Vector2f t1 = v1.head<2>();
    Vector2f t2 = v2.head<2>();

    for (int x = min_x; x <= max_x; x++) {
        for (int y = min_y; y <= max_y; y++) {

            // 2. 算重心坐标 (这一步跟你之前的一模一样)
            auto [alpha, beta, gamma] = MathUtils::compute_barycentric((float)x + 0.5f, (float)y + 0.5f, t0, t1, t2);

            if (alpha >= 0 && beta >= 0 && gamma >= 0) {
                // 3. 插值算出 Z
                float z = alpha * v0.z() + beta * v1.z() + gamma * v2.z();
                int index = y * shadow_width + x;

                // 4. 【核心】只比大小，更近就记录，不画颜色！
                if (z < shadow_buffer[index]) {
                    shadow_buffer[index] = z;
                }
            }
        }
    }
}

// 可视化 (把 float 数组转成图片给你看)
cv::Mat Renderer::get_shadow_image() {
    cv::Mat img(shadow_height, shadow_width, CV_8UC1); // 黑白图
    for (int i = 0; i < shadow_width * shadow_height; i++) {
        float z = shadow_buffer[i];
        if (z > 10000.0f) { // 无穷大
            img.data[i] = 0; // 黑背景
        }
        else {
            // 这里为了看见东西，随便映射一下，假设深度在 -50 到 50 之间
            // 实际可能需要根据你的场景调整
            // 越近越白(255)，越远越黑(0)
            // Z 范围大概是 [-1, 1] (NDC)
            int val = (int)((z * 0.5f + 0.5f) * 255);
            img.data[i] = (unsigned char)std::clamp(val, 0, 255);
        }
    }
    return img;
}

void Renderer::clear_shadow() {
    shadow_buffer.resize(shadow_width * shadow_height);
    std::fill(shadow_buffer.begin(), shadow_buffer.end(), std::numeric_limits<float>::max());
}