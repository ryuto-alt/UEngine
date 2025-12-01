#pragma once

#include "../Core/Component.h"
#include "LuaState.h"
#include <memory>
#include <string>

namespace UnoEngine {

// Luaスクリプトコンポーネント
// GameObjectにアタッチしてLuaスクリプトを実行する
class LuaScriptComponent : public Component {
public:
    LuaScriptComponent() = default;
    ~LuaScriptComponent() override = default;

    // Component ライフサイクル
    void Awake() override;
    void Start() override;
    void OnUpdate(float deltaTime) override;
    void OnDestroy() override;

    // スクリプト設定
    void SetScriptPath(std::string_view path);
    [[nodiscard]] const std::string& GetScriptPath() const { return scriptPath_; }

    // スクリプトの読み込み
    [[nodiscard]] bool LoadScript();
    [[nodiscard]] bool ReloadScript();

    // エラー取得
    [[nodiscard]] bool HasError() const;
    [[nodiscard]] const std::optional<LuaError>& GetLastError() const;

    // プロパティ操作（Inspector用）
    [[nodiscard]] std::vector<ScriptProperty> GetProperties() const;
    void SetProperty(std::string_view name, const ScriptPropertyValue& value);

    // LuaState直接アクセス（上級者向け）
    [[nodiscard]] LuaState* GetLuaState() { return luaState_.get(); }
    [[nodiscard]] const LuaState* GetLuaState() const { return luaState_.get(); }

    // ホットリロード確認・実行
    void CheckHotReload();

private:
    // エンジンAPIをLuaに公開
    void BindEngineAPI();

private:
    std::unique_ptr<LuaState> luaState_;
    std::string scriptPath_;
    bool scriptLoaded_ = false;
    bool awakeCalledInLua_ = false;
    bool startCalledInLua_ = false;
};

} // namespace UnoEngine
