#pragma once

namespace UnoEngine {

struct ImportOptions {
    // Coordinate system conversion
    bool convertCoordinateSystem = true;

    // Additional rotation (degrees) applied during import
    float rotationX = 0.0f;
    float rotationY = 0.0f;
    float rotationZ = 0.0f;

    // Scale factor
    float scale = 1.0f;

    // Animation import
    bool importAnimations = true;

    // Material import
    bool importMaterials = true;

    // Texture import
    bool importTextures = true;

    // Generate tangents if not present
    bool generateTangents = true;

    // Factory methods for common presets
    static ImportOptions Default() {
        return {};
    }

    static ImportOptions ForGltf() {
        ImportOptions opts;
        opts.rotationX = -90.0f;  // Stand up Y-up models to Z-up
        return opts;
    }

    static ImportOptions ForGltfNoRotation() {
        ImportOptions opts;
        opts.convertCoordinateSystem = false;
        return opts;
    }

    static ImportOptions ForFbx() {
        ImportOptions opts;
        // FBX typically uses Y-up right-handed, similar to glTF
        opts.rotationX = -90.0f;
        return opts;
    }

    static ImportOptions ForObj() {
        ImportOptions opts;
        // OBJ can vary, default to no rotation
        opts.convertCoordinateSystem = false;
        return opts;
    }
};

} // namespace UnoEngine
