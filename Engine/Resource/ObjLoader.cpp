#include "ObjLoader.h"
#include <fstream>
#include <sstream>
#include <cassert>
#include <unordered_map>
#include <filesystem>
#include <Windows.h>
#include <iostream>

namespace UnoEngine {

namespace {

void LogObjError(const std::string& message, const std::string& file, int line) {
    std::string fullMessage = "[OBJ Loader Error] " + message + "\n  File: " + file + "\n  Line: " + std::to_string(line);
    std::cerr << fullMessage << std::endl;
    OutputDebugStringA((fullMessage + "\n").c_str());
    MessageBoxA(nullptr, fullMessage.c_str(), "OBJ Loading Error", MB_OK | MB_ICONERROR);
}

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

    if (!file.is_open()) {
        std::string errorMsg = "Failed to open OBJ file\n  File path: " + filepath + "\n\nPlease check:\n  1. File exists\n  2. File path is correct\n  3. File is not locked by another program";
        LogObjError(errorMsg, filepath, 0);
        throw std::runtime_error("Failed to open OBJ file: " + filepath);
    }

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

            // 頂点トークンをすべて収集
            std::vector<std::string> faceTokens;
            while (ss >> token) {
                faceTokens.push_back(token);
            }

            // 三角形かチェック
            if (faceTokens.size() < 3) {
                std::string errorMsg = "Face has only " + std::to_string(faceTokens.size()) + " vertices (minimum 3 required)";
                LogObjError(errorMsg, filepath, lineNumber);
                throw std::runtime_error(errorMsg);
            }

            if (faceTokens.size() > 3) {
                std::stringstream detailMsg;
                detailMsg << "Face has " << faceTokens.size() << " vertices (only triangles are supported)\n";
                detailMsg << "  Face data: f";
                for (const auto& t : faceTokens) {
                    detailMsg << " " << t;
                }
                detailMsg << "\n\n";
                detailMsg << "Solution: Triangulate your mesh in your 3D software:\n";
                detailMsg << "  - Blender: Select All > Triangulate Faces (Ctrl+T)\n";
                detailMsg << "  - Maya: Mesh > Triangulate\n";
                detailMsg << "  - 3ds Max: Edit Poly > Turn to Triangles";

                LogObjError(detailMsg.str(), filepath, lineNumber);
                throw std::runtime_error("OBJ file contains non-triangulated faces");
            }

            // 三角形の場合のみ処理
            for (int i = 0; i < 3; ++i) {
                ParseFace(faceTokens[i], faceIndices[i]);
            }

            for (const auto& faceIndex : faceIndices) {
                auto it = vertexCache.find(faceIndex);
                if (it != vertexCache.end()) {
                    indices.push_back(it->second);
                } else {
                    Vertex vertex;

                    if (faceIndex.positionIndex < 0 || faceIndex.positionIndex >= static_cast<int32>(positions.size())) {
                        std::stringstream errorMsg;
                        errorMsg << "Invalid vertex position index: " << faceIndex.positionIndex << "\n";
                        errorMsg << "  Valid range: 0 to " << (positions.size() - 1) << "\n";
                        errorMsg << "  Total positions defined: " << positions.size();
                        LogObjError(errorMsg.str(), filepath, lineNumber);
                        throw std::runtime_error("Invalid vertex index in face");
                    }

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

    if (vertices.empty() || indices.empty()) {
        std::stringstream errorMsg;
        errorMsg << "OBJ file contains no geometry\n";
        errorMsg << "  Vertices: " << vertices.size() << "\n";
        errorMsg << "  Indices: " << indices.size() << "\n";
        errorMsg << "  Positions (v): " << positions.size() << "\n";
        errorMsg << "  UVs (vt): " << uvs.size() << "\n";
        errorMsg << "  Normals (vn): " << normals.size() << "\n\n";
        errorMsg << "Please check:\n";
        errorMsg << "  - File contains 'f' (face) definitions\n";
        errorMsg << "  - Faces reference valid vertices";
        LogObjError(errorMsg.str(), filepath, lineNumber);
        throw std::runtime_error("OBJ file contains no geometry");
    }

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
