#include "ObjLoader.h"
#include <fstream>
#include <sstream>
#include <cassert>
#include <unordered_map>
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

}

Mesh ObjLoader::Load(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
                     const std::string& filepath) {
    std::ifstream file(filepath);
    assert(file.is_open() && "Failed to open OBJ file");

    std::vector<Vector3> positions;
    std::vector<Vector2> uvs;
    std::vector<Vector3> normals;
    std::vector<Vertex> vertices;
    std::vector<uint32> indices;
    std::unordered_map<FaceIndex, uint32, FaceIndexHash> vertexCache;

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

        if (type == "v") {
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
                    vertex.position = positions[faceIndex.positionIndex];

                    if (faceIndex.uvIndex >= 0 && faceIndex.uvIndex < static_cast<int32>(uvs.size())) {
                        vertex.uv = uvs[faceIndex.uvIndex];
                    } else {
                        vertex.uv = Vector2::Zero();
                    }

                    if (faceIndex.normalIndex >= 0 && faceIndex.normalIndex < static_cast<int32>(normals.size())) {
                        vertex.normal = normals[faceIndex.normalIndex];
                    } else {
                        vertex.normal = Vector3::UnitY();
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
    sprintf_s(debugMsg, "OBJ Loaded: %s - %zu vertices, %zu indices\n",
             name.c_str(), vertices.size(), indices.size());
    OutputDebugStringA(debugMsg);

    Mesh mesh;
    mesh.Create(device, commandList, vertices, indices, name);
    return mesh;
}

} // namespace UnoEngine
