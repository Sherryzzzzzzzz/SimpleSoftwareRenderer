#pragma once
#include <opencv2/opencv.hpp>
#include <Eigen/Dense>
#include <vector>
#include <cmath>
#include "Skybox.h" 

using namespace cv;
using namespace Eigen;

class Renderer {
public:
    // 构造函数 (统一用 int)
    Renderer(int w, int h);
    ~Renderer();

    void clear(Skybox& skybox, const Vector3f& camera_pos, const Vector3f& camera_target);

    // 画线
    void draw_line(Vector2i p0, Vector2i p1, Vector3i color);

    // 画实心三角形 (3D版本，带深度)
    void rasterize_triangle(Vector3f v0, Vector3f v1, Vector3f v2, Vector3i color);

    // 画彩色三角形 (2D版本，测试用)
    void rasterize_triangle_test(Vector2i v0, Vector2i v1, Vector2i v2);

    Mat& get_frame_buffer();
	const std::vector<float>& get_z_buffer() const { return z_buffer; }
    void rasterize_triangle(Vector3f v0, Vector3f v1, Vector3f v2,
        Vector2f uv0, Vector2f uv1, Vector2f uv2,
        Vector3f n0, Vector3f n1, Vector3f n2,
        Vector4f s0, Vector4f s1, Vector4f s2,
        const cv::Mat& texture,bool is_face, float alpha=1.0f);

    void init_shadow_buffer(int w, int h);

    void rasterize_shadow(Vector3f v0, Vector3f v1, Vector3f v2);

    cv::Mat get_shadow_image();

    void clear_shadow();

private:
    int width;  // 【修正】用 int
    int height; // 【修正】用 int
    Mat frame_buffer;
    std::vector<float> z_buffer; // 深度缓冲

    int shadow_width;
    int shadow_height;
    std::vector<float> shadow_buffer;

    // 画点 (统一用 int)
    void set_pixel(int x, int y, const Vector3i& color);
};