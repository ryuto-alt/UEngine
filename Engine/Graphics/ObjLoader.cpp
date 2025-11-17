#include "ObjLoader.h"
#include <fstream>
#include <sstream>
#include <cassert>
#include <unordered_map>
#include <filesystem>
#include <Windows.h>

namespace UnoEngine {

namespace {

struct FaceIndex {
    int32 positionIndex;
    int32 uvIndex;
    int32 normalIndex;

    bool operator==(const FaceIndex& other) const {
        return positionIndex == other.positionIndex &&
               uvIndex == other.uvIndex &&
               normalIndex == other.normalIndex;
    }
};

struct FaceIndexHash {
    size_t operator()(const FaceIndex& f) const {
        return std::hash<int32>()(f.positionIndex) ^
               (std::hash<int32>()(f.uvIndex) << 1) ^
               (std::hash<int32>()(f.normalIndex) << 2);
    }
};

void ParseFace(const std::string& token, FaceIndex& outIndex) {
    outIndex = { -1, -1, -1 };

    std::stringstream ss(token);
    std::string indexStr;

    std::getline(ss, indexStr, '/');
    if (!indexStr.empty()) {
        outIndex.positionIndex = std::stoi(indexStr) - 1;
    }

    if (std::getline(ss, indexStr, '/')) {
        if (!indexStr.empty()) {
            outIndex.uvIndex = std::stoi(indexStr) - 1;
        }

        if (std::getline(ss, indexStr, '/')) {
            if (!indexStr.empty()) {
                outIndex.normalIndex = std::stoi(indexStr) - 1;
            }
        }
    }
}

Vector3 CalculateFaceNormal(const Vector3& v0, const Vector3& v1, const Vector3& v2) {
    const Vector3 edge1 = Vector3(
        v1.GetX() - v0.GetX(),
        v1.GetY() - v0.GetY(),
        v1.GetZ() - v0.GetZ()
    );
    const Vector3 edge2 = Vector3(
        v2.GetX() - v0.GetX(),
        v2.GetY() - v0.GetY(),
        v2.GetZ() - v0.GetZ()
    );
    return edge1.Cross(edge2).Normalize();
}


std::unordered_map<std::string, MaterialData> LoadMTL(const std::string& mtlPath, const std::string& baseDirectory) {
    std::unordered_map<std::string, MaterialData> materials;
    
    std::ifstream file(mtlPath);
    if (!file.is_open()) {
        return materials;
    }

    MaterialData* currentMaterial = nullptr;
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "newmtl") {
            std::string name;
            std::getline(ss >> std::ws, name);
            currentMaterial = &materials[name];
            currentMaterial->name = name;
        }
        else if (currentMaterial) {
            if (type == "Ka") {
                ss >> currentMaterial->ambient[0] >> currentMaterial->ambient[1] >> currentMaterial->ambient[2];
            }
            else if (type == "Kd") {
                ss >> currentMaterial->diffuse[0] >> currentMaterial->diffuse[1] >> currentMaterial->diffuse[2];
            }
            else if (type == "Ks") {
                ss >> currentMaterial->specular[0] >> currentMaterial->specular[1] >> currentMaterial->specular[2];
            }
            else if (type == "Ke") {
                ss >> currentMaterial->emissive[0] >> currentMaterial->emissive[1] >> currentMaterial->emissive[2];
            }
            else if (type == "Ns") {
                ss >> currentMaterial->shininess;
            }
            else if (type == "d") {
                ss >> currentMaterial->opacity;
            }
            else if (type == "map_Kd") {
                std::string texPath;
                std::getline(ss >> std::ws, texPath);
                
                namespace fs = std::filesystem;
                fs::path absolutePath(texPath);
                
                if (absolutePath.is_absolute()) {
                    currentMaterial->diffuseTexturePath = absolutePath.filename().string();
                } else {
                    currentMaterial->diffuseTexturePath = texPath;
                }
            }
        }
    }

    return materials;
}

}

Mesh ObjLoader::Load(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                     const std::string& filepath) {
    auto* device = graphics->GetDevice();
    std::ifstream file(filepath);
    assert(file.is_open() && "Failed to open OBJ file");

    namespace fs = std::filesystem;
    const fs::path objPath(filepath);
    const std::string baseDirectory = objPath.parent_path().string();

    std::vector<Vector3> positions;
    std::vector<Vector2> uvs;
    std::vector<Vector3> normals;
    std::vector<Vertex> vertices;
    std::vector<uint32> indices;
    std::unordered_map<FaceIndex, uint32, FaceIndexHash> vertexCache;
    
    std::unordered_map<std::string, MaterialData> materials;
    std::string currentMaterialName;
    MaterialData* activeMaterial = nullptr;

    std::string line;
    uint32 lineNumber = 0;

    while (std::getline(file, line)) {
        ++lineNumber;

        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "mtllib") {
            std::string mtlFilename;
            ss >> mtlFilename;
            const std::string mtlPath = baseDirectory + "/" + mtlFilename;
            materials = LoadMTL(mtlPath, baseDirectory);
        }
        else if (type == "usemtl") {
            std::getline(ss >> std::ws, currentMaterialName);
            auto it = materials.find(currentMaterialName);
            if (it != materials.end()) {
                activeMaterial = &it->second;
            }
        }
        else if (type == "v") {
            float x, y, z;
            ss >> x >> y >> z;
            positions.emplace_back(x, y, z);
        }
        else if (type == "vt") {
            float u, v;
            ss >> u >> v;
            uvs.emplace_back(u, v);
        }
        else if (type == "vn") {
            float x, y, z;
            ss >> x >> y >> z;
            normals.emplace_back(x, y, z);
        }
        else if (type == "f") {
            FaceIndex faceIndices[3];
            std::string token;

            for (int i = 0; i < 3; ++i) {
                assert(ss >> token && "Face must have 3 vertices");
                ParseFace(token, faceIndices[i]);
            }

            std::string extra;
            assert(!(ss >> extra) && "Only triangles are supported");

            for (const auto& faceIndex : faceIndices) {
                auto it = vertexCache.find(faceIndex);
                if (it != vertexCache.end()) {
                    indices.push_back(it->second);
                } else {
                    Vertex vertex;

                    assert(faceIndex.positionIndex >= 0 &&
                           faceIndex.positionIndex < static_cast<int32>(positions.size()));
                    const Vector3& pos = positions[faceIndex.positionIndex];
                    vertex.px = pos.GetX();
                    vertex.py = pos.GetY();
                    vertex.pz = pos.GetZ();

                    if (faceIndex.uvIndex >= 0 && faceIndex.uvIndex < static_cast<int32>(uvs.size())) {
                        const Vector2& uv = uvs[faceIndex.uvIndex];
                        vertex.u = uv.GetX();
                        vertex.v = uv.GetY();
                    } else {
                        vertex.u = 0.0f;
                        vertex.v = 0.0f;
                    }

                    if (faceIndex.normalIndex >= 0 && faceIndex.normalIndex < static_cast<int32>(normals.size())) {
                        const Vector3& norm = normals[faceIndex.normalIndex];
                        vertex.nx = norm.GetX();
                        vertex.ny = norm.GetY();
                        vertex.nz = norm.GetZ();
                    } else {
                        vertex.nx = 0.0f;
                        vertex.ny = 1.0f;
                        vertex.nz = 0.0f;
                    }

                    const uint32 vertexIndex = static_cast<uint32>(vertices.size());
                    vertices.push_back(vertex);
                    indices.push_back(vertexIndex);
                    vertexCache[faceIndex] = vertexIndex;
                }
            }
        }
    }

    assert(!vertices.empty() && !indices.empty() && "OBJ file contains no geometry");

    const size_t lastSlash = filepath.find_last_of("/\\");
    const std::string name = (lastSlash != std::string::npos)
        ? filepath.substr(lastSlash + 1)
        : filepath;

    char debugMsg[512];
    sprintf_s(debugMsg, "OBJ Loaded: %s - %zu vertices, %zu indices, %zu materials\n",
             name.c_str(), vertices.size(), indices.size(), materials.size());
    OutputDebugStringA(debugMsg);

    Mesh mesh;
    mesh.Create(device, commandList, vertices, indices, name);
    
    if (activeMaterial && !materials.empty()) {
        mesh.LoadMaterial(*activeMaterial, graphics, commandList, baseDirectory, 0);
    }
    
    return mesh;
}

} // namespace UnoEngine
