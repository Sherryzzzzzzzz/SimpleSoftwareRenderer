#include "Renderer.h"
#include "MathUtils.h" 
#include <algorithm>   
#include <cmath> 
#include <limits> 

using namespace MathUtils;

// --- 构造函数 ---
Renderer::Renderer(int w, int h) : width(w), height(h) {
    frame_buffer = Mat(height, width, CV_8UC3);
    z_buffer.resize(width * height);
    std::fill(z_buffer.begin(), z_buffer.end(), std::numeric_limits<float>::infinity());
}

Renderer::~Renderer() {
}

// 替换原来的 clear()
void Renderer::clear(Skybox& skybox, const Vector3f& camera_pos, const Vector3f& camera_target) {
    // 1. 清空 Z-Buffer
    std::fill(z_buffer.begin(), z_buffer.end(), std::numeric_limits<float>::infinity());

    // 2. 如果没加载天空盒，就填个渐变色保底
    if (!skybox.is_loaded) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float t = (float)y / height;
                // 简单的蓝白渐变
                unsigned char b = (unsigned char)(235 * (1 - t) + 240 * t);
                unsigned char g = (unsigned char)(206 * (1 - t) + 240 * t);
                unsigned char r = (unsigned char)(135 * (1 - t) + 240 * t);
                frame_buffer.at<Vec3b>(y, x) = Vec3b(b, g, r);
            }
        }
        return;
    }

    // 3. 计算相机的基向量 (Camera Basis)
    // 这和 MathUtils::get_view_matrix 的逻辑是一样的
    Vector3f front = (camera_target - camera_pos).normalized();
    Vector3f up_world(0, 1, 0);
    Vector3f right = front.cross(up_world).normalized();
    Vector3f cam_up = right.cross(front).normalized();

    // 4. 视锥参数 (和 main 里的 projection 保持一致)
    float fov = 45.0f;
    float aspect = (float)width / height;
    float scale = std::tan(fov * 0.5f * 3.14159f / 180.f);

    // 5. 遍历每个像素 (逆向光线追踪)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {

            // 把屏幕坐标映射到 NDC [-1, 1]
            // x: -1 (左) -> 1 (右)
            // y:  1 (顶) -> -1 (底)  <-- 注意 Y 轴方向
            float ndc_x = (2.0f * (x + 0.5f) / width - 1.0f) * aspect * scale;
            float ndc_y = (1.0f - 2.0f * (y + 0.5f) / height) * scale;

            // 算出射线方向 (World Space)
            // Dir = ndc_x * Right + ndc_y * Up + Front
            Vector3f dir = (ndc_x * right + ndc_y * cam_up + front).normalized();

            // 采样天空盒
            Vector3i color = skybox.sample(dir);

            // 写入 Framebuffer
            frame_buffer.at<Vec3b>(y, x) = Vec3b(color.z(), color.y(), color.x());
        }
    }
}

Mat& Renderer::get_frame_buffer() {
    return frame_buffer;
}

// --- 画点 ---
void Renderer::set_pixel(int x, int y, const Vector3i& color) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    int cv_y = height - 1 - y;
    frame_buffer.at<Vec3b>(cv_y, x) = Vec3b(color.z(), color.y(), color.x());
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

// --- 阴影图光栅化 (只记深度) ---
void Renderer::rasterize_shadow(Vector3f v0, Vector3f v1, Vector3f v2) {
    int min_x = (int)std::min({ v0.x(), v1.x(), v2.x() });
    int max_x = (int)std::max({ v0.x(), v1.x(), v2.x() });
    int min_y = (int)std::min({ v0.y(), v1.y(), v2.y() });
    int max_y = (int)std::max({ v0.y(), v1.y(), v2.y() });

    min_x = std::max(0, min_x); max_x = std::min(shadow_width - 1, max_x);
    min_y = std::max(0, min_y); max_y = std::min(shadow_height - 1, max_y);

    Vector2f t0 = v0.head<2>();
    Vector2f t1 = v1.head<2>();
    Vector2f t2 = v2.head<2>();

    for (int x = min_x; x <= max_x; x++) {
        for (int y = min_y; y <= max_y; y++) {
            auto [a, b, c] = MathUtils::compute_barycentric((float)x + 0.5f, (float)y + 0.5f, t0, t1, t2);
            // 双面渲染支持
            if ((a >= 0 && b >= 0 && c >= 0) || (a <= 0 && b <= 0 && c <= 0)) {
                float z = a * v0.z() + b * v1.z() + c * v2.z();
                int index = y * shadow_width + x;
                if (z < shadow_buffer[index]) {
                    shadow_buffer[index] = z;
                }
            }
        }
    }
}

// --- 核心渲染函数 ---
void Renderer::rasterize_triangle(Vector3f v0, Vector3f v1, Vector3f v2,
    Vector2f uv0, Vector2f uv1, Vector2f uv2,
    Vector3f n0, Vector3f n1, Vector3f n2,
    Vector4f s0, Vector4f s1, Vector4f s2,
    const cv::Mat& texture, bool is_face, float alpha) {

    // 1. 包围盒
    int min_x = (int)std::min({ v0.x(), v1.x(), v2.x() });
    int max_x = (int)std::max({ v0.x(), v1.x(), v2.x() });
    int min_y = (int)std::min({ v0.y(), v1.y(), v2.y() });
    int max_y = (int)std::max({ v0.y(), v1.y(), v2.y() });

    min_x = std::max(0, min_x); max_x = std::min(width - 1, max_x);
    min_y = std::max(0, min_y); max_y = std::min(height - 1, max_y);

    Vector2f t0 = v0.head<2>();
    Vector2f t1 = v1.head<2>();
    Vector2f t2 = v2.head<2>();

    for (int x = min_x; x <= max_x; x++) {
        for (int y = min_y; y <= max_y; y++) {

            // 2. 重心坐标
            auto [a, b, c] = MathUtils::compute_barycentric((float)x + 0.5f, (float)y + 0.5f, t0, t1, t2);

            // 支持双面渲染 (只要符号一致)
            if ((a >= 0 && b >= 0 && c >= 0) || (a <= 0 && b <= 0 && c <= 0)) {

                // 3. 插值 Z
                float z_current = a * v0.z() + b * v1.z() + c * v2.z();
                int index = y * width + x;

                // 4. 深度测试
                if (z_current < z_buffer[index]) {

                    // === A. 准备纹理颜色 ===
                    float u = a * uv0.x() + b * uv1.x() + c * uv2.x();
                    float v = a * uv0.y() + b * uv1.y() + c * uv2.y();
                    u = std::min(1.0f, std::max(0.0f, u));
                    v = std::min(1.0f, std::max(0.0f, v));

                    Vector3f tex_color;

                    if (texture.empty()) {
                        // 棋盘格逻辑 (地板)
                        float scale = 10.0f;
                        float check_u = u * scale;
                        float check_v = v * scale;
                        int parity = (int)(std::floor(check_u) + std::floor(check_v)) % 2;

                        if (parity == 0) tex_color = Vector3f(240, 240, 240);
                        else             tex_color = Vector3f(180, 180, 190);
                    }
                    else {
                        // 正常读图逻辑
                        int tex_x = (int)(u * (texture.cols - 1));
                        int tex_y = (int)((1.0f - v) * (texture.rows - 1));

                        // 统一读 3 通道 BGR，安全第一
                        cv::Vec3b color_bgr = texture.at<cv::Vec3b>(tex_y, tex_x);
                        tex_color = Vector3f(color_bgr[2], color_bgr[1], color_bgr[0]);
                    }

                    // === B. 阴影查表 (PCF) ===
                    float shadow_factor = 1.0f;

                    // 脸部无阴影，地板阴影淡一点(0.7)，身体阴影(0.5)
                    float shadow_intensity = texture.empty() ? 0.7f : 0.5f;

                    
                        Vector4f s_pos = a * s0 + b * s1 + c * s2;
                        Vector3f s_ndc = s_pos.head<3>() / s_pos.w();
                        float su = s_ndc.x() * 0.5f + 0.5f;
                        float sv = s_ndc.y() * 0.5f + 0.5f;
                        float sz = s_ndc.z() * 0.5f + 0.5f;

                        if (su >= 0 && su < 1 && sv >= 0 && sv < 1) {
                            int sx = (int)(su * (shadow_width - 1));
                            int sy = (int)(sv * (shadow_height - 1));
                            int sidx = sy * shadow_width + sx;

                            // Shadow Bias (0.005) 防止自阴影
                            if (sz - 0.005f > shadow_buffer[sidx]) {
                                shadow_factor = shadow_intensity;
                            }
                        }
                    

                    // === C. 卡通光照 (Toon Shading) ===
                    Vector3f normal = (a * n0 + b * n1 + c * n2).normalized();
                    Vector3f light_dir = Vector3f(1.0f, 1.0f, 1.0f).normalized();

                    float NdotL = std::max(0.0f, normal.dot(light_dir));

                    Vector3f light_color;

                    if (is_face) {
                        light_color = Vector3f(1.0f, 1.0f, 1.0f); // 脸部恒亮
                    }
                    else {
                        // 身体/衣服：二值化光照
                        if (NdotL > 0.5f) {
                            light_color = Vector3f(1.0f, 1.0f, 1.0f); // 亮部
                        }
                        else {
                            // 🟢【画质提升】暗部不要死黑，用蓝紫色环境光
                            light_color = Vector3f(0.6f, 0.6f, 0.75f);
                        }
                    }

                    // 叠加阴影
                    // 如果在阴影里，强制把颜色变暗 (乘上冷色调阴影)
                    if (shadow_factor < 0.9f) {
                        light_color = light_color.cwiseProduct(Vector3f(0.6f, 0.6f, 0.75f));
                    }

                    // === D. 边缘光 (Rim Light) ===
                    Vector3f rim_color(0, 0, 0);
                    if (!is_face && alpha > 0.9f) {
                        Vector3f view_dir(0, 0, 1);
                        float NdotV = normal.dot(view_dir);
                        float rim = std::pow(1.0f - std::max(0.0f, NdotV), 4.0f);
                        if (rim > 0.4f) rim = 1.0f; else rim = 0.0f; // 硬边缘
                        rim_color = Vector3f(50, 50, 80) * rim; // 淡淡的蓝光
                    }

                    // === E. 组合最终颜色 ===
                    // 纹理 * 光照 + 边缘光
                    Vector3f final_color = tex_color.cwiseProduct(light_color) + rim_color;

                    // === F. 写入像素 ===

                    // 不透明 (Body/Face) -> 写 Z，覆盖颜色
                    if (alpha > 0.9f) {
                        z_buffer[index] = z_current;

                        final_color.x() = std::min(255.0f, final_color.x());
                        final_color.y() = std::min(255.0f, final_color.y());
                        final_color.z() = std::min(255.0f, final_color.z());
                        set_pixel(x, y, final_color.cast<int>());
                    }
                    // 半透明 (Glass) -> 不写 Z，混合颜色
                    else {
                        int cv_y = height - 1 - y;
                        cv::Vec3b bg_val = frame_buffer.at<cv::Vec3b>(cv_y, x);
                        Vector3f bg_color(bg_val[2], bg_val[1], bg_val[0]);

                        Vector3f blended = final_color * alpha + bg_color * (1.0f - alpha);

                        blended.x() = std::min(255.0f, blended.x());
                        blended.y() = std::min(255.0f, blended.y());
                        blended.z() = std::min(255.0f, blended.z());
                        set_pixel(x, y, blended.cast<int>());
                    }
                }
            }
        }
    }
}

// 辅助函数
void Renderer::init_shadow_buffer(int w, int h) {
    shadow_width = w;
    shadow_height = h;
    shadow_buffer.resize(w * h);
    std::fill(shadow_buffer.begin(), shadow_buffer.end(), std::numeric_limits<float>::max());
}

void Renderer::clear_shadow() {
    if (shadow_buffer.size() != shadow_width * shadow_height) {
        shadow_buffer.resize(shadow_width * shadow_height);
    }
    std::fill(shadow_buffer.begin(), shadow_buffer.end(), std::numeric_limits<float>::max());
}

