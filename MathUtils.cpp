#include "MathUtils.h"

using namespace Eigen;

// 1. 基础版本
float MathUtils::cross_product_2d(float x0, float y0, float x1, float y1, float x, float y) {
    return (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0);
}

// 2. Eigen 重载版本
float MathUtils::cross_product_2d(const Vector2f& a, const Vector2f& b, const Vector2f& p) {
    return cross_product_2d(a.x(), a.y(), b.x(), b.y(), p.x(), p.y());
}

// 3. 判断是否在内部
bool MathUtils::is_inside(int x, int y, const Vector2f& v0, const Vector2f& v1, const Vector2f& v2) {
    // 【修正】必须把整数坐标 (x,y) 转成 float 向量，否则 Eigen 报错
    Vector2f p((float)x, (float)y);

    float cp1 = cross_product_2d(v0, v1, p);
    float cp2 = cross_product_2d(v1, v2, p);
    float cp3 = cross_product_2d(v2, v0, p);

    return (cp1 >= 0 && cp2 >= 0 && cp3 >= 0) || (cp1 <= 0 && cp2 <= 0 && cp3 <= 0);
}

// 4. 计算重心坐标
std::tuple<float, float, float> MathUtils::compute_barycentric(float x, float y, const Vector2f& v0, const Vector2f& v1, const Vector2f& v2) {
    // 计算总面积
    // 注意：cross_product_2d 需要三个参数 (A, B, P)，这里我们把 v2 当作 P 来算 v0-v1-v2 的面积
    float area_total = cross_product_2d(v0, v1, v2);

    // 【修正】构造当前点 p (float)
    Vector2f p(x, y);

    // 计算分量 (注意顺序：v1-v2-p 对应 alpha/v0)
    float alpha = cross_product_2d(v1, v2, p) / area_total;
    float beta = cross_product_2d(v2, v0, p) / area_total;
    float gamma = 1.0f - alpha - beta;

    return { alpha, beta, gamma };
}

// 1. 模型矩阵
Eigen::Matrix4f MathUtils::get_model_matrix(float angle_x, float angle_y, float angle_z) {
    // 1. 转弧度
    float rad_x = angle_x * MY_PI / 180.0f;
    float rad_y = angle_y * MY_PI / 180.0f;
    float rad_z = angle_z * MY_PI / 180.0f;

    // 2. 绕 X 轴旋转矩阵
    Eigen::Matrix4f rotation_x = Eigen::Matrix4f::Identity();
    rotation_x << 1, 0, 0, 0,
        0, std::cos(rad_x), -std::sin(rad_x), 0,
        0, std::sin(rad_x), std::cos(rad_x), 0,
        0, 0, 0, 1;

    // 3. 绕 Y 轴旋转矩阵 (注意 sin 的符号位置！)
    Eigen::Matrix4f rotation_y = Eigen::Matrix4f::Identity();
    rotation_y << std::cos(rad_y), 0, std::sin(rad_y), 0,
        0, 1, 0, 0,
        -std::sin(rad_y), 0, std::cos(rad_y), 0,
        0, 0, 0, 1;

    // 4. 绕 Z 轴旋转矩阵
    Eigen::Matrix4f rotation_z = Eigen::Matrix4f::Identity();
    rotation_z << std::cos(rad_z), -std::sin(rad_z), 0, 0,
        std::sin(rad_z), std::cos(rad_z), 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1;

    // 5. 组合旋转
    // 矩阵乘法顺序很重要：先转 X，再转 Y，最后转 Z (也可以反过来，效果不同)
    // Model = Rz * Ry * Rx
    Eigen::Matrix4f model = rotation_z * rotation_y * rotation_x;

    return model;
}

// 2. 视图矩阵
Matrix4f MathUtils::get_view_matrix(Vector3f eye_pos) {
    Matrix4f view = Matrix4f::Identity();
    Matrix4f translate;
    // 这里的逻辑是：相机往后退 = 物体往相反方向动
    translate << 1, 0, 0, -eye_pos.x(),
        0, 1, 0, -eye_pos.y(),
        0, 0, 1, -eye_pos.z(),
        0, 0, 0, 1;

    view = translate * view;
    return view;
}

// 3. 投影矩阵 (这就是报错的那个函数)
Matrix4f MathUtils::get_projection_matrix(float eye_fov, float aspect_ratio, float zNear, float zFar) {
    Matrix4f projection = Matrix4f::Zero();
    float tanHalfFovy = std::tan((eye_fov * MY_PI / 180.0f) / 2.0f);

    projection(0, 0) = 1.0f / (aspect_ratio * tanHalfFovy);
    projection(1, 1) = 1.0f / tanHalfFovy;
    projection(2, 2) = -(zFar + zNear) / (zFar - zNear);
    projection(2, 3) = -(2.0f * zFar * zNear) / (zFar - zNear);
    projection(3, 2) = -1.0f;

    return projection;
}

Eigen::Matrix4f MathUtils::get_ortho_matrix(float left, float right, float bottom, float top, float zNear, float zFar) {
    Eigen::Matrix4f ortho = Eigen::Matrix4f::Identity();

    // 缩放
    ortho(0, 0) = 2.0f / (right - left);
    ortho(1, 1) = 2.0f / (top - bottom);
    ortho(2, 2) = 2.0f / (zNear - zFar); // 注意这里通常是负的

    // 平移
    ortho(0, 3) = -(right + left) / (right - left);
    ortho(1, 3) = -(top + bottom) / (top - bottom);
    ortho(2, 3) = -(zNear + zFar) / (zNear - zFar);

    return ortho;
}