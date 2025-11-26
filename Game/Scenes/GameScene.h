#pragma once

#include "../../Engine/Core/Scene.h"
#include "../../Engine/Resource/SkinnedModelImporter.h"
#include "../../Engine/Rendering/SkinnedRenderItem.h"
#include "../../Engine/Math/Matrix.h"
#include <vector>

#ifdef _DEBUG
#include "../UI/EditorUI.h"
#endif

namespace UnoEngine {

class GameScene : public Scene {
public:
    void OnLoad() override;
    void OnUpdate(float deltaTime) override;
    void OnRender(RenderView& view) override;
    void OnImGui() override;

    GameObject* GetPlayer() const { return player_; }

    // スキンメッシュアイテムの取得
    std::vector<SkinnedRenderItem> GetSkinnedRenderItems() const;

#ifdef _DEBUG
    // EditorUIへのアクセス (Debug builds only)
    EditorUI* GetEditorUI() { return &editorUI_; }
#endif

private:
    GameObject* player_ = nullptr;

    // Skinned model data
    SkinnedModelData skinnedModel_;
    std::vector<Matrix4x4> boneMatrices_;

#ifdef _DEBUG
    EditorUI editorUI_;
#endif
};

} // namespace UnoEngine
