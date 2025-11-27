#pragma once

#include "../Core/Component.h"
#include "../Math/BoundingVolume.h"

namespace UnoEngine {

class Material;

class MeshRendererBase : public Component {
public:
    MeshRendererBase() = default;
    virtual ~MeshRendererBase() = default;

    // Material management
    void SetMaterial(Material* material) { materialOverride_ = material; }
    Material* GetMaterial() const { return materialOverride_ ? materialOverride_ : defaultMaterial_; }
    Material* GetMaterialOverride() const { return materialOverride_; }
    bool HasMaterialOverride() const { return materialOverride_ != nullptr; }

    // Bounds (for culling)
    const BoundingBox& GetBounds() const { return bounds_; }
    const BoundingSphere& GetBoundingSphere() const { return boundingSphere_; }

    // Visibility
    bool IsVisible() const { return isVisible_; }
    void SetVisible(bool visible) { isVisible_ = visible; }

    // Rendering layer (for render order)
    uint32 GetRenderQueue() const { return renderQueue_; }
    void SetRenderQueue(uint32 queue) { renderQueue_ = queue; }

protected:
    void SetDefaultMaterial(Material* material) { defaultMaterial_ = material; }
    
    void UpdateBounds(const BoundingBox& box) {
        bounds_ = box;
        boundingSphere_ = BoundingSphere::CreateFromBox(box);
    }

    Material* defaultMaterial_ = nullptr;
    Material* materialOverride_ = nullptr;
    BoundingBox bounds_;
    BoundingSphere boundingSphere_;
    bool isVisible_ = true;
    uint32 renderQueue_ = 2000;  // Default: Geometry
};

// Render queue constants
namespace RenderQueue {
    constexpr uint32 Background = 1000;
    constexpr uint32 Geometry = 2000;
    constexpr uint32 AlphaTest = 2450;
    constexpr uint32 Transparent = 3000;
    constexpr uint32 Overlay = 4000;
}

} // namespace UnoEngine
