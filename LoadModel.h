#pragma once
#include <vector>
#include <string>
#include <Eigen/Dense>

using namespace Eigen;

namespace LoadModel {

    // 一个部件（比如头发）
    struct SubMesh {
        std::vector<Vector3f> vertices;
        std::vector<Vector2f> texcoords;
        std::vector<Vector3f> normals;
        int texture_id; // 这个部件对应第几张图？
        bool is_face;
    };

    // 整个模型
    struct Model {
        std::vector<SubMesh> meshes;       // 模型由很多部件组成
        std::vector<std::string> texture_paths; // 存所有贴图的文件名
    };

    std::string clean_path(std::string path);
    bool load_obj(const std::string& path, const std::string& base_dir, Model& model);
}