#pragma once
#include <Eigen/Dense>
#include <tuple> // 必须包含这个

namespace MathUtils {
    const float MY_PI = 3.1415926f;
    // 基础叉积 (float)
    float cross_product_2d(float x0, float y0, float x1, float y1, float x, float y);

    // 基础叉积 (Eigen Vector2f)
    float cross_product_2d(const Eigen::Vector2f& a, const Eigen::Vector2f& b, const Eigen::Vector2f& p);

    // 判断点是否在三角形内
    bool is_inside(int x, int y, const Eigen::Vector2f& v0, const Eigen::Vector2f& v1, const Eigen::Vector2f& v2);

    // 计算重心坐标
    std::tuple<float, float, float> compute_barycentric(float x, float y, const Eigen::Vector2f& v0, const Eigen::Vector2f& v1, const Eigen::Vector2f& v2);
    Eigen::Matrix4f get_projection_matrix(float eye_fov, float aspect_ratio, float zNear, float zFar);
    Eigen::Matrix4f get_view_matrix(Eigen::Vector3f eye_pos);
    Eigen::Matrix4f get_model_matrix(float angle_x, float angle_y, float angle_z);
    Eigen::Matrix4f get_ortho_matrix(float left, float right, float bottom, float top, float zNear, float zFar);
};