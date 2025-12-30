#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <filesystem> 
#include <cmath> // 需要这个算 cos/sin

#include "Renderer.h"
#include "LoadModel.h"
#include "MathUtils.h"
#include <Eigen/Dense>
#include <opencv2/opencv.hpp>

using namespace cv;
using namespace Eigen;
using namespace LoadModel;
using namespace MathUtils;

#define SHADOW_WIDTH 2048
#define SHADOW_HEIGHT 2048
const int WIDTH = 700;
const int HEIGHT = 700;

// ==========================================
// 🟢 1. 鼠标交互状态管理
// ==========================================
struct MouseState {
    bool left_down = false;
    bool right_down = false;
    int last_x = 0;
    int last_y = 0;

    // 模型状态 (由鼠标控制)
    float model_x = 0.0f;
    float model_y = 0.0f;
    float angle_x = 0.0f;
    float angle_y = 0.0f;
    int scroll_delta = 0;
};

// 鼠标回调函数
void onMouse(int event, int x, int y, int flags, void* userdata) {
    MouseState* state = (MouseState*)userdata;

    if (event == EVENT_LBUTTONDOWN) {
        state->left_down = true;
        state->last_x = x;
        state->last_y = y;
    }
    else if (event == EVENT_LBUTTONUP) {
        state->left_down = false;
    }
    else if (event == EVENT_RBUTTONDOWN) {
        state->right_down = true;
        state->last_x = x;
        state->last_y = y;
    }
    else if (event == EVENT_RBUTTONUP) {
        state->right_down = false;
    }
    else if (event == EVENT_MOUSEMOVE) {
        int dx = x - state->last_x;
        int dy = y - state->last_y;

        // 左键拖拽 -> 移动模型 (X/Y 平面)
        if (state->left_down) {
            float move_speed = 0.05f;
            state->model_x += dx * move_speed;
            state->model_y -= dy * move_speed; // 屏幕Y向下，世界Y向上，所以反过来
        }

        // 右键拖拽 -> 旋转模型
        if (state->right_down) {
            float rot_speed = 0.5f;
            state->angle_y += dx * rot_speed;
            state->angle_x += dy * rot_speed;
        }

        state->last_x = x;
        state->last_y = y;
    }

    else if (event == EVENT_MOUSEWHEEL) {
        state->scroll_delta += getMouseWheelDelta(flags)/2;
    }
}

int main(int argc, char** argv) {
    // 1. 获取路径
    std::string obj_path;
    if (argc > 1) {
        obj_path = argv[1];
    }
    else {
        std::cout << "Drag .OBJ file here: ";
        std::getline(std::cin, obj_path);
    }
    obj_path = clean_path(obj_path);

    std::string base_dir = std::filesystem::path(obj_path).parent_path().string();

    // 2. 加载模型
    Model my_model;
    if (!load_obj(obj_path, base_dir, my_model)) {
        system("pause");
        return -1;
    }

    std::cout << "Model loaded! Total SubMeshes: " << my_model.meshes.size() << std::endl;

    // 3. 加载贴图
    std::vector<cv::Mat> texture_library;
    cv::Mat default_tex(1024, 1024, CV_8UC3, Scalar(255, 255, 255));

    for (const auto& path : my_model.texture_paths) {
        if (path.empty()) {
            texture_library.push_back(default_tex);
            continue;
        }

        std::cout << "Loading texture: " << path << std::endl;
        cv::Mat img = cv::imread(path);
        if (!img.empty()) {
            texture_library.push_back(img);
        }
        else {
            std::cout << "Failed to load: " << path << " (Using white fallback)" << std::endl;
            texture_library.push_back(default_tex);
        }
    }

    // 4. 自动缩放逻辑
    float min_x = std::numeric_limits<float>::max(); float max_x = std::numeric_limits<float>::lowest();
    float min_y = std::numeric_limits<float>::max(); float max_y = std::numeric_limits<float>::lowest();
    float min_z = std::numeric_limits<float>::max(); float max_z = std::numeric_limits<float>::lowest();

    for (const auto& mesh : my_model.meshes) {
        for (const auto& v : mesh.vertices) {
            min_x = std::min(min_x, v.x()); max_x = std::max(max_x, v.x());
            min_y = std::min(min_y, v.y()); max_y = std::max(max_y, v.y());
            min_z = std::min(min_z, v.z()); max_z = std::max(max_z, v.z());
        }
    }

    float center_x = (min_x + max_x) / 2.0f;
    float center_y = (min_y + max_y) / 2.0f;
    float center_z = (min_z + max_z) / 2.0f;

    float max_dim = std::max({ max_x - min_x, max_y - min_y, max_z - min_z });
    float target_size = 10.0f;
    float scale = target_size / max_dim;

    // 初始化渲染器
    Renderer rst(WIDTH, HEIGHT);

    // 🟢 初始化交互状态
    MouseState mouse_state;
    namedWindow("LuckyStar Renderer", WINDOW_AUTOSIZE);
    setMouseCallback("LuckyStar Renderer", onMouse, &mouse_state);

    // 相机与灯光变量
    Vector3f camera_pos(0.0f, 0.0f, 20.0f);
    // 键盘控制相机的旋转角度
    float cam_pitch = 0.0f;
    float cam_yaw = 0.0f;

    Vector3f light_pos(20.0f, 20.0f, 20.0f);
    rst.init_shadow_buffer(1024, 1024);

    // =============================================================
    // 🟢 初始化天空盒 (支持单张全景图)
    // =============================================================
    Skybox skybox;
    std::cout << "Drag [Panorama Image] here (Press ENTER to skip): ";
    std::string sky_path;
    std::getline(std::cin, sky_path);
    sky_path = clean_path(sky_path);
    if (!sky_path.empty()) skybox.load(sky_path);

    while (true) {
        Vector3f target_pos(0.0f, 3.0f, 0.0f);
        rst.clear(skybox, camera_pos, target_pos);

        // --- 1. 处理键盘输入 (控制相机) ---
        int key = waitKey(10);
        float move_speed = 0.5f;
        float rot_speed = 2.0f;

        // 相机移动 (WASDQE)
        if (key == 'e') camera_pos.y() += move_speed;
        if (key == 'q') camera_pos.y() -= move_speed;
        if (key == 'a') camera_pos.x() -= move_speed;
        if (key == 'd') camera_pos.x() += move_speed;
        if (key == 'w') camera_pos.z() -= move_speed;
        if (key == 's') camera_pos.z() += move_speed;

        // 相机旋转 (IJKL) - 模拟摇头和点头
        if (key == 'i') cam_pitch += rot_speed;
        if (key == 'k') cam_pitch -= rot_speed;
        if (key == 'j') cam_yaw += rot_speed;
        if (key == 'l') cam_yaw -= rot_speed;

        if (key == 27) break; // ESC 退出

        if (mouse_state.scroll_delta != 0) {
            float zoom_speed = 0.05f;
            camera_pos.z() -= mouse_state.scroll_delta * zoom_speed;
            if (camera_pos.z() < 1.0f) camera_pos.z() = 1.0f;
            mouse_state.scroll_delta = 0;
        }

        // --- 2. 矩阵计算 ---

        // 🟢 A. 计算 Model 矩阵 (由鼠标控制)
        // 先获取旋转矩阵
        Matrix4f model_rot = MathUtils::get_model_matrix(mouse_state.angle_x, mouse_state.angle_y, 0);
        // 再手动构建位移矩阵
        Matrix4f model_trans = Matrix4f::Identity();
        model_trans(0, 3) = mouse_state.model_x;
        model_trans(1, 3) = mouse_state.model_y;
        model_trans(2, 3) = 0.0f; // 鼠标只控制平面移动
        // 组合: 先旋转再位移
        Matrix4f model = model_trans * model_rot;
        Matrix4f normal_matrix = model_rot; // 法线只受旋转影响

        // 🟢 B. 计算 View 矩阵 (由键盘控制)
        // 1. 获取基础位移矩阵 (你要求的1个参数版本)
        Matrix4f view_trans = MathUtils::get_view_matrix(camera_pos);
        // 2. 额外叠加相机的旋转 (模拟人头转动)
        // 注意：相机的旋转相当于世界的反向旋转
        Matrix4f view_rot = MathUtils::get_model_matrix(-cam_pitch, -cam_yaw, 0);
        // 组合 View = Rot * Trans
        Matrix4f view = view_rot * view_trans;

        // C. Proj 矩阵
        Matrix4f proj = MathUtils::get_projection_matrix(45.0f, 1.0f, 0.1f, 2000.0f);

        Matrix4f camera_mvp;

        // D. Light 矩阵
        Matrix4f l_view = MathUtils::get_view_matrix(light_pos);
        Matrix4f l_proj = MathUtils::get_ortho_matrix(-30, 30, -30, 30, 0.1f, 100.0f);
        Matrix4f light_mvp = l_proj * l_view * model;

        // =========================================================
        // Pass 1: Shadow Map
        // =========================================================
        rst.clear_shadow();

        for (const auto& mesh : my_model.meshes) {
            std::string tex_path = "";
            if (mesh.texture_id >= 0 && mesh.texture_id < my_model.texture_paths.size()) {
                tex_path = my_model.texture_paths[mesh.texture_id];
            }
            std::transform(tex_path.begin(), tex_path.end(), tex_path.begin(), ::tolower);

            // 跳过眼镜和玻璃
            if (tex_path.find("megane") != std::string::npos ||
                tex_path.find("glass") != std::string::npos) continue;

            for (int i = 0; i < mesh.vertices.size(); i += 3) {
                Vector3f p_light[3];
                for (int j = 0; j < 3; j++) {
                    Vector3f v_local = (mesh.vertices[i + j] - Vector3f(center_x, center_y, center_z)) * scale;

                    Vector4f v_clip = light_mvp * Vector4f(v_local.x(), v_local.y(), v_local.z(), 1.0f);
                    Vector3f v_ndc = v_clip.head<3>() / v_clip.w();

                    // 🟢【修改 1】使用全局变量 SHADOW_WIDTH，不要写死 1024
                    p_light[j].x() = 0.5f * SHADOW_WIDTH * (v_ndc.x() + 1.0f);
                    p_light[j].y() = 0.5f * SHADOW_HEIGHT * (v_ndc.y() + 1.0f);

                    // 🟢【修改 2】将 Z 值从 NDC[-1,1] 映射到 [0,1]
                    // 这能让深度值的分布更合理，减少 Z-Fighting 带来的锯齿和斑点
                    p_light[j].z() = v_ndc.z() * 0.5f + 0.5f;
                }
                rst.rasterize_shadow(p_light[0], p_light[1], p_light[2]);
            }
        }

        // =========================================================
        // Pass 2.1: 画地板
        // =========================================================
        camera_mvp = proj * view * Matrix4f::Identity(); // 地板不受模型矩阵影响
        auto draw_3d_line = [&](Vector3f start, Vector3f end, Scalar color, int thickness = 1) {
            // 1. MVP 变换
            Vector4f s_clip = camera_mvp * Vector4f(start.x(), start.y(), start.z(), 1.0f);
            Vector4f e_clip = camera_mvp * Vector4f(end.x(), end.y(), end.z(), 1.0f);

            // 2. 简单裁剪 (如果点在相机后面，就不画，防止画面乱飞)
            if (s_clip.w() < 0.1f || e_clip.w() < 0.1f) return;

            // 3. 透视除法
            Vector3f s_ndc = s_clip.head<3>() / s_clip.w();
            Vector3f e_ndc = e_clip.head<3>() / e_clip.w();

            // 4. 视口变换
            Point p1((s_ndc.x() + 1.0f) * 0.5f * WIDTH, (1.0f - (s_ndc.y() + 1.0f) * 0.5f) * HEIGHT);
            Point p2((e_ndc.x() + 1.0f) * 0.5f * WIDTH, (1.0f - (e_ndc.y() + 1.0f) * 0.5f) * HEIGHT);

            // 5. 调用 OpenCV 画线 (抗锯齿 LINE_AA)
            cv::line(rst.get_frame_buffer(), p1, p2, color, thickness, cv::LINE_AA);
            };
        int grid_size = 20;       // 网格范围 (-20 到 20)
        float grid_step = 1.0f;   // 每一格多大 (1.0 米)

        float floor_level = (min_y - center_y) * scale;

        Scalar grid_color(180, 180, 180); // 浅灰色 (BGR)
        Scalar axis_color(100, 100, 100); // 深灰色 (中心线)

        for (int i = -grid_size; i <= grid_size; i++) {
            float pos = i * grid_step;

            // 颜色区分：中心线画深一点，普通线画浅一点
            Scalar col = (i == 0) ? axis_color : grid_color;
            int thick = (i == 0) ? 2 : 1;

            // 画平行于 X 轴的线 (从左到右)
            draw_3d_line(Vector3f(-grid_size, floor_level, pos),
                Vector3f(grid_size, floor_level, pos), col, thick);

            // 画平行于 Z 轴的线 (从前到后)
            draw_3d_line(Vector3f(pos, floor_level, -grid_size),
                Vector3f(pos, floor_level, grid_size), col, thick);
        }
        // =========================================================
        // 🟢 2. 绘制坐标轴与刻度尺 (Gizmo)
        // =========================================================
        float axis_len = 1000.0f; // 轴长度
        float tick_len = 0.2f;  // 刻度线长度

        // 原点 (在地板上)
        Vector3f O(0, floor_level, 0);

        // --- X轴 (红色) ---
        draw_3d_line(O, O + Vector3f(axis_len, 0, 0), Scalar(0, 0, 255), 2);
        // 画 X 轴刻度
        for (float i = 1.0f; i <= axis_len; i += 1.0f) {
            Vector3f tick_pos = O + Vector3f(i, 0, 0);
            // 刻度竖着画一下
            draw_3d_line(tick_pos, tick_pos + Vector3f(0, tick_len, 0), Scalar(0, 0, 255), 1);
        }

        // --- Y轴 (绿色) ---
        draw_3d_line(O, O + Vector3f(0, axis_len, 0), Scalar(0, 255, 0), 2);
        // 画 Y 轴刻度
        for (float i = 1.0f; i <= axis_len; i += 1.0f) {
            Vector3f tick_pos = O + Vector3f(0, i, 0);
            // 刻度横着画一下
            draw_3d_line(tick_pos, tick_pos + Vector3f(tick_len, 0, 0), Scalar(0, 255, 0), 1);
        }

        // --- Z轴 (蓝色) ---
        draw_3d_line(O, O + Vector3f(0, 0, axis_len), Scalar(255, 0, 0), 2);
        // 画 Z 轴刻度
        for (float i = 1.0f; i <= axis_len; i += 1.0f) {
            Vector3f tick_pos = O + Vector3f(0, 0, i);
            // 刻度竖着画一下
            draw_3d_line(tick_pos, tick_pos + Vector3f(0, tick_len, 0), Scalar(255, 0, 0), 1);
        }

        // =========================================================
        // Pass 2.2: 画人物实体 (Alpha=1.0)
        // =========================================================
		camera_mvp = proj * view * model;
        for (const auto& mesh : my_model.meshes) {
            cv::Mat current_texture = default_tex;
            if (mesh.texture_id >= 0 && mesh.texture_id < texture_library.size()) {
                current_texture = texture_library[mesh.texture_id];
            }

            std::string tex_path = "";
            if (mesh.texture_id >= 0 && mesh.texture_id < my_model.texture_paths.size()) {
                tex_path = my_model.texture_paths[mesh.texture_id];
            }
            std::transform(tex_path.begin(), tex_path.end(), tex_path.begin(), ::tolower);

            bool is_glass = (tex_path.find("megane") != std::string::npos) ||
                (tex_path.find("glass") != std::string::npos);
            if (is_glass) continue;

            for (int i = 0; i < mesh.vertices.size(); i += 3) {
                Vector3f v[3] = { mesh.vertices[i], mesh.vertices[i + 1], mesh.vertices[i + 2] };
                Vector2f uv[3] = { mesh.texcoords[i], mesh.texcoords[i + 1], mesh.texcoords[i + 2] };
                Vector3f n[3] = { mesh.normals[i], mesh.normals[i + 1], mesh.normals[i + 2] };
                Vector3f p_screen[3]; Vector3f n_trans[3]; Vector4f p_shadow[3];

                for (int j = 0; j < 3; j++) {
                    Vector3f v_local = (v[j] - Vector3f(center_x, center_y, center_z)) * scale;

                    Vector4f v_clip = camera_mvp * Vector4f(v_local.x(), v_local.y(), v_local.z(), 1.0f);
                    Vector3f v_ndc = v_clip.head<3>() / v_clip.w();
                    p_screen[j].x() = 0.5f * WIDTH * (v_ndc.x() + 1.0f);
                    p_screen[j].y() = 0.5f * HEIGHT * (v_ndc.y() + 1.0f);
                    p_screen[j].z() = v_ndc.z();

                    Vector4f n_temp = normal_matrix * Vector4f(n[j].x(), n[j].y(), n[j].z(), 0.0f);
                    n_trans[j] = n_temp.head<3>().normalized();
                    p_shadow[j] = light_mvp * Vector4f(v_local.x(), v_local.y(), v_local.z(), 1.0f);
                }
                rst.rasterize_triangle(p_screen[0], p_screen[1], p_screen[2],
                    uv[0], uv[1], uv[2], n_trans[0], n_trans[1], n_trans[2],
                    p_shadow[0], p_shadow[1], p_shadow[2],
                    current_texture, mesh.is_face, 1.0f);
            }
        }
        // =========================================================
        // 后期处理 (描边)
        // =========================================================
        const std::vector<float>& z_buf = rst.get_z_buffer();
        cv::Mat frame = rst.get_frame_buffer();
        float bg_depth = 4000.0f;
        float edge_threshold = 0.001f;

        for (int y = 0; y < HEIGHT - 1; y++) {
            int z_y = HEIGHT - 1 - y;
            for (int x = 0; x < WIDTH - 1; x++) {
                int idx = z_y * WIDTH + x;
                float z_center = z_buf[idx];
                if (z_center > bg_depth) continue;

                int idx_right = z_y * WIDTH + (x + 1);
                int z_y_down = z_y - 1;
                if (z_y_down < 0) z_y_down = 0;
                int idx_down = z_y_down * WIDTH + x;

                float z_right = z_buf[idx_right];
                float z_down = z_buf[idx_down];

                float diff = 0.0f;
                bool is_silhouette = false;
                if (z_right > bg_depth || z_down > bg_depth) {
                    diff = 100.0f;
                    is_silhouette = true;
                }
                else {
                    float dx = std::abs(z_center - z_right);
                    float dy = std::abs(z_center - z_down);
                    diff = dx + dy;
                }

                if (diff > edge_threshold) {
                    frame.at<cv::Vec3b>(y, x) = cv::Vec3b(0, 0, 0);
                    if (is_silhouette) {
                        frame.at<cv::Vec3b>(y, x + 1) = cv::Vec3b(0, 0, 0);
                        frame.at<cv::Vec3b>(y + 1, x) = cv::Vec3b(0, 0, 0);
                    }
                }
            }
        }

        imshow("LuckyStar Renderer", frame);
    }

    return 0;
}