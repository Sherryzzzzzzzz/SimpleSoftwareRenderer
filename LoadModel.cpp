#include "LoadModel.h"
#include <iostream>
#include <map>
#include <algorithm> // 用于 transform 转小写

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

namespace LoadModel {

    std::string clean_path(std::string path) {
        if (path.empty()) return "";
        path.erase(0, path.find_first_not_of(" \t\n\r"));
        path.erase(path.find_last_not_of(" \t\n\r") + 1);
        if (path.front() == '"') path.erase(0, 1);
        if (path.back() == '"') path.pop_back();
        return path;
    }

    bool load_obj(const std::string& path, const std::string& base_dir, Model& model) {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        std::cout << "Loading model: " << path << "..." << std::endl;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(), base_dir.c_str());

        if (!warn.empty()) std::cout << "Warn: " << warn << std::endl;
        if (!err.empty()) std::cerr << "Err: " << err << std::endl;
        if (!ret) return false;

        // --- 🟢 步骤 1：预处理材质，标记哪些 ID 是脸 ---
        std::vector<bool> material_is_face_flags;

        std::cout << "------ Material Inspection ------" << std::endl;

        for (const auto& mat : materials) {
            // 1.1 处理贴图路径 (保持不变)
            if (!mat.diffuse_texname.empty()) {
                std::string raw = mat.diffuse_texname;
                std::string filename;
                size_t slash = raw.find_last_of("/\\");
                if (slash != std::string::npos) filename = raw.substr(slash + 1);
                else filename = raw;
                model.texture_paths.push_back(base_dir + "/" + filename);
            }
            else {
                model.texture_paths.push_back("");
            }

            // 1.2 🟢【升级版】双重检测：查名字 + 查图片名
            std::string mat_name = mat.name;
            std::string tex_name = mat.diffuse_texname;

            // 全部转小写，方便匹配
            std::string mat_name_lower = mat_name;
            std::string tex_name_lower = tex_name;
            std::transform(mat_name_lower.begin(), mat_name_lower.end(), mat_name_lower.begin(), ::tolower);
            std::transform(tex_name_lower.begin(), tex_name_lower.end(), tex_name_lower.begin(), ::tolower);

            bool is_face = false;

            // 扩充关键词库：kao(颜), face(脸), skin(皮肤), eye(眼), hitomi(瞳)
            // 只要材质名 OR 贴图名里包含这些，就算脸
            if (mat_name_lower.find("kao") != std::string::npos ||
                mat_name_lower.find("face") != std::string::npos ||
                mat_name_lower.find("skin") != std::string::npos ||
                mat_name_lower.find("eye") != std::string::npos ||
                // 检查贴图名
                tex_name_lower.find("kao") != std::string::npos ||
                tex_name_lower.find("face") != std::string::npos ||
                tex_name_lower.find("hitomi") != std::string::npos || // 瞳孔
                tex_name_lower.find("eye") != std::string::npos) {

                is_face = true;
            }

            // 🔴 打印出来给你看，到底是哪个材质被选中了
            std::cout << "ID: " << material_is_face_flags.size()
                << " | Name: [" << mat_name << "] "
                << " | Tex: [" << tex_name << "] "
                << " -> " << (is_face ? "[FACE]" : "[Body]") << std::endl;

            material_is_face_flags.push_back(is_face);
        }
        std::cout << "---------------------------------" << std::endl;
        // --- 🟢 步骤 2：按材质拆分网格 (不要在这里过滤！) ---
        std::map<int, SubMesh> sorted_meshes;

        for (const auto& shape : shapes) {
            size_t index_offset = 0;
            for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {

                int mat_id = -1;
                if (!shape.mesh.material_ids.empty()) {
                    mat_id = shape.mesh.material_ids[f];
                }

                // 只要有材质ID，就无条件加入对应的桶，千万不要写 if(is_face)

                for (size_t v = 0; v < 3; v++) {
                    tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

                    Vector3f vert(
                        attrib.vertices[3 * idx.vertex_index + 0],
                        attrib.vertices[3 * idx.vertex_index + 1],
                        attrib.vertices[3 * idx.vertex_index + 2]
                    );

                    Vector2f tex(0, 0);
                    if (idx.texcoord_index >= 0) {
                        tex.x() = attrib.texcoords[2 * idx.texcoord_index + 0];
                        tex.y() = attrib.texcoords[2 * idx.texcoord_index + 1];
                    }

                    Vector3f norm(0, 0, 1);
                    if (idx.normal_index >= 0) {
                        norm.x() = attrib.normals[3 * idx.normal_index + 0];
                        norm.y() = attrib.normals[3 * idx.normal_index + 1];
                        norm.z() = attrib.normals[3 * idx.normal_index + 2];
                    }

                    sorted_meshes[mat_id].vertices.push_back(vert);
                    sorted_meshes[mat_id].texcoords.push_back(tex);
                    sorted_meshes[mat_id].normals.push_back(norm);
                }
                index_offset += 3;
            }
        }

        // --- 🟢 步骤 3：组装最终模型，并打上 is_face 标记 ---
        for (auto& pair : sorted_meshes) {
            int mat_id = pair.first;
            SubMesh& mesh = pair.second;

            mesh.texture_id = mat_id;

            // 查表：这个材质ID是不是脸？
            if (mat_id >= 0 && mat_id < material_is_face_flags.size()) {
                mesh.is_face = material_is_face_flags[mat_id];
            }
            else {
                mesh.is_face = false;
            }

            model.meshes.push_back(mesh);
        }

        return true;
    }
}