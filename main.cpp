#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <filesystem> 

#include "Renderer.h"
#include "LoadModel.h"
#include "MathUtils.h"
#include <Eigen/Dense>
#include <opencv2/opencv.hpp>

using namespace cv;
using namespace Eigen;
using namespace LoadModel;
using namespace MathUtils;

const int WIDTH = 700;
const int HEIGHT = 700;

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

    // 🟢【修改1】移出循环：加载完成只打印一次
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
    // 先计算包围盒
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

    // 计算中心点
    float center_x = (min_x + max_x) / 2.0f;
    float center_y = (min_y + max_y) / 2.0f;
    float center_z = (min_z + max_z) / 2.0f;

    float max_dim = std::max({ max_x - min_x, max_y - min_y, max_z - min_z });
    float target_size = 5.0f; // 让模型高度大概为 5 个单位
    float scale = target_size / max_dim;

    // 初始化渲染器
    Renderer rst(WIDTH, HEIGHT);
    float angle_x = 0.0f;
    float angle_y = 0.0f;

    Vector3f camera_pos(0.0f, 0.0f, 10.0f);
    Vector3f light_pos(20.0f, 20.0f, 20.0f);
    rst.init_shadow_buffer(1024, 1024);

    while (true) {
        rst.clear();

        // --- 1. 处理输入 ---
        // 🟢【修改3】加上参数 10，防止卡死
        int key = waitKey(10);

        if (key == 'w') camera_pos.y() += 0.5f;
        if (key == 's') camera_pos.y() -= 0.5f;
        if (key == 'a') camera_pos.x() -= 0.5f;
        if (key == 'd') camera_pos.x() += 0.5f;
        if (key == 'q') camera_pos.z() -= 0.5f;
        if (key == 'e') camera_pos.z() += 0.5f;

        if (key == 'i') angle_x += 5.0f;
        if (key == 'k') angle_x -= 5.0f;
        if (key == 'j') angle_y += 5.0f;
        if (key == 'l') angle_y -= 5.0f;

        if (key == 27) break; // ESC 退出

        Matrix4f model = MathUtils::get_model_matrix(angle_x, angle_y, 0);
        Matrix4f view = MathUtils::get_view_matrix(camera_pos);
        Matrix4f proj = MathUtils::get_projection_matrix(45.0f, 1.0f, 0.1f, 100.0f);
        Matrix4f camera_mvp = proj * view * model;
        Matrix4f normal_matrix = model;

        // 2. 灯光的 MVP (Light Space)
        // 这里的参数必须和 Pass 1 保持严格一致！
        Matrix4f l_view = MathUtils::get_view_matrix(light_pos);
        // 正交投影范围：根据模型大小调整 (-30, 30 是经验值，如果阴影被切断了就调大)
        Matrix4f l_proj = MathUtils::get_ortho_matrix(-30, 30, -30, 30, 0.1f, 100.0f);
        Matrix4f light_mvp = l_proj * l_view * model; // 注意这里也乘了 model

        rst.clear_shadow(); // 只清空 shadow buffer，不清空 frame buffer

        for (const auto& mesh : my_model.meshes) {
            for (int i = 0; i < mesh.vertices.size(); i += 3) {
                Vector3f v[3] = { mesh.vertices[i], mesh.vertices[i + 1], mesh.vertices[i + 2] };
                Vector3f p_light[3];

                for (int j = 0; j < 3; j++) {
                    Vector3f v_local = (v[j] - Vector3f(center_x, center_y, center_z)) * scale;
                    // 用 Light MVP 变换
                    Vector4f v_clip = light_mvp * Vector4f(v_local.x(), v_local.y(), v_local.z(), 1.0f);
                    Vector3f v_ndc = v_clip.head<3>() / v_clip.w();
                    // 视口变换到 1024x1024
                    p_light[j].x() = 0.5f * 1024 * (v_ndc.x() + 1.0f);
                    p_light[j].y() = 0.5f * 1024 * (v_ndc.y() + 1.0f);
                    p_light[j].z() = v_ndc.z();
                }
                rst.rasterize_shadow(p_light[0], p_light[1], p_light[2]);
            }
        }

        for (const auto& mesh : my_model.meshes) {
            cv::Mat current_texture = default_tex;
            if (mesh.texture_id >= 0 && mesh.texture_id < texture_library.size()) {
                current_texture = texture_library[mesh.texture_id];
            }

            std::string tex_path = "";
            if (mesh.texture_id >= 0 && mesh.texture_id < my_model.texture_paths.size()) {
                tex_path = my_model.texture_paths[mesh.texture_id];
            }
            // 转小写
            std::transform(tex_path.begin(), tex_path.end(), tex_path.begin(), ::tolower);

            // 2. 判断：如果是眼镜，这一轮先【跳过】不画！
            bool is_glass = (tex_path.find("megane") != std::string::npos) ||
                (tex_path.find("glass") != std::string::npos);
            if (is_glass) continue;

            // 3. 获取贴图数据
            cv::Mat current_tex = default_tex;
            if (mesh.texture_id >= 0 && mesh.texture_id < texture_library.size()) {
                current_tex = texture_library[mesh.texture_id];
            }

            for (int i = 0; i < mesh.vertices.size(); i += 3) {
                Vector3f v[3] = { mesh.vertices[i], mesh.vertices[i + 1], mesh.vertices[i + 2] };
                Vector2f uv[3] = { mesh.texcoords[i], mesh.texcoords[i + 1], mesh.texcoords[i + 2] };
                Vector3f n[3] = { mesh.normals[i], mesh.normals[i + 1], mesh.normals[i + 2] };

                Vector3f p_screen[3]; // 屏幕坐标
                Vector4f p_shadow[3]; // 阴影坐标 (Light Space)

                for (int j = 0; j < 3; j++) {
                    Vector3f v_local = (v[j] - Vector3f(center_x, center_y, center_z)) * scale;

                    // A. 算屏幕坐标 (给相机看)
                    Vector4f v_clip = camera_mvp * Vector4f(v_local.x(), v_local.y(), v_local.z(), 1.0f);
                    Vector3f v_ndc = v_clip.head<3>() / v_clip.w();
                    p_screen[j].x() = 0.5f * WIDTH * (v_ndc.x() + 1.0f);
                    p_screen[j].y() = 0.5f * HEIGHT * (v_ndc.y() + 1.0f);
                    p_screen[j].z() = v_ndc.z();

                    // B. 算阴影坐标 (用来查 Shadow Map)
                
                    p_shadow[j] = light_mvp * Vector4f(v_local.x(), v_local.y(), v_local.z(), 1.0f);

                    // 法线
                    Vector4f n_temp = normal_matrix * Vector4f(n[j].x(), n[j].y(), n[j].z(), 0.0f);
                    n[j] = n_temp.head<3>().normalized();
                }

                rst.rasterize_triangle(p_screen[0], p_screen[1], p_screen[2],
                    uv[0], uv[1], uv[2],
                    n[0], n[1], n[2],
                    p_shadow[0], p_shadow[1], p_shadow[2], // 传入阴影坐标
                    current_texture, mesh.is_face);
            }
        }
        imshow("LuckyStar Renderer", rst.get_frame_buffer());
    }

    return 0;
}