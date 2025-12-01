#include "LuaScriptComponent.h"
#include "../Core/GameObject.h"
#include "../Core/Logger.h"

namespace UnoEngine {

void LuaScriptComponent::Awake() {
    // LuaStateがまだ作成されていない場合は作成
    if (!luaState_) {
        luaState_ = std::make_unique<LuaState>();
        if (!luaState_->Initialize()) {
            Logger::Error("[LuaScriptComponent] Failed to initialize LuaState");
            return;
        }
    }

    // スクリプトパスが設定されている場合は読み込み
    if (!scriptPath_.empty() && !scriptLoaded_) {
        (void)LoadScript();
    }

    // スクリプトが読み込まれていればAwakeを呼ぶ
    if (scriptLoaded_ && !awakeCalledInLua_) {
        luaState_->CallAwake();
        awakeCalledInLua_ = true;
    }
}

void LuaScriptComponent::Start() {
    if (scriptLoaded_ && !startCalledInLua_) {
        luaState_->CallStart();
        startCalledInLua_ = true;
    }
}

void LuaScriptComponent::OnUpdate(float deltaTime) {
    // ホットリロードチェック
    CheckHotReload();

    if (scriptLoaded_) {
        luaState_->CallUpdate(deltaTime);
    }
}

void LuaScriptComponent::OnDestroy() {
    if (scriptLoaded_) {
        luaState_->CallOnDestroy();
    }
}

void LuaScriptComponent::SetScriptPath(std::string_view path) {
    scriptPath_ = std::string(path);
    
    // 既にLuaStateが初期化されていればスクリプトを読み込む
    if (luaState_ && luaState_->GetScriptPath().empty()) {
        (void)LoadScript();
    }
}

bool LuaScriptComponent::LoadScript() {
    if (scriptPath_.empty()) {
        Logger::Warning("[LuaScriptComponent] Script path is empty");
        return false;
    }

    if (!luaState_) {
        luaState_ = std::make_unique<LuaState>();
        if (!luaState_->Initialize()) {
            return false;
        }
    }

    // エンジンAPIをバインド
    BindEngineAPI();

    // スクリプト読み込み
    scriptLoaded_ = luaState_->LoadScript(scriptPath_);
    
    if (scriptLoaded_) {
        Logger::Info("[LuaScriptComponent] Script loaded: {}", scriptPath_);
    }

    return scriptLoaded_;
}

bool LuaScriptComponent::ReloadScript() {
    if (scriptPath_.empty()) return false;

    // 状態をリセット
    awakeCalledInLua_ = false;
    startCalledInLua_ = false;
    scriptLoaded_ = false;

    // LuaStateを再作成
    luaState_ = std::make_unique<LuaState>();
    if (!luaState_->Initialize()) {
        return false;
    }

    // 再読み込み
    if (!LoadScript()) {
        return false;
    }

    // ライフサイクル関数を呼び直す
    if (IsAwakeCalled()) {
        luaState_->CallAwake();
        awakeCalledInLua_ = true;
    }
    if (HasStarted()) {
        luaState_->CallStart();
        startCalledInLua_ = true;
    }

    return true;
}

bool LuaScriptComponent::HasError() const {
    return luaState_ && luaState_->GetLastError().has_value();
}

const std::optional<LuaError>& LuaScriptComponent::GetLastError() const {
    static const std::optional<LuaError> empty;
    if (!luaState_) return empty;
    return luaState_->GetLastError();
}

std::vector<ScriptProperty> LuaScriptComponent::GetProperties() const {
    if (!luaState_) return {};
    return luaState_->GetPublicProperties();
}

void LuaScriptComponent::SetProperty(std::string_view name, const ScriptPropertyValue& value) {
    if (luaState_) {
        luaState_->SetProperty(name, value);
    }
}

void LuaScriptComponent::CheckHotReload() {
    if (!luaState_ || scriptPath_.empty()) return;

    if (luaState_->CheckAndReload()) {
        // リロード後、エンジンAPIを再バインド
        BindEngineAPI();
        
        // ライフサイクル関数を呼び直す
        if (awakeCalledInLua_) {
            luaState_->CallAwake();
        }
        if (startCalledInLua_) {
            luaState_->CallStart();
        }

        Logger::Info("[LuaScriptComponent] Script hot-reloaded: {}", scriptPath_);
    }
}

void LuaScriptComponent::BindEngineAPI() {
    if (!luaState_) return;

    auto& lua = luaState_->GetState();
    auto* gameObject = GetGameObject();

    // ===== gameObject =====
    if (gameObject) {
        lua["gameObject"] = lua.create_table_with(
            "name", gameObject->GetName(),
            "getName", [gameObject]() { return gameObject->GetName(); },
            "setName", [gameObject](const std::string& name) { gameObject->SetName(name); },
            "isActive", [gameObject]() { return gameObject->IsActive(); },
            "setActive", [gameObject](bool active) { gameObject->SetActive(active); }
        );

        // ===== transform =====
        auto& transform = gameObject->GetTransform();
        lua["transform"] = lua.create_table_with(
            // Position
            "getPosition", [&transform]() {
                auto pos = transform.GetLocalPosition();
                return std::make_tuple(pos.GetX(), pos.GetY(), pos.GetZ());
            },
            "setPosition", [&transform](float x, float y, float z) {
                transform.SetLocalPosition(Vector3(x, y, z));
            },
            "translate", [&transform](float x, float y, float z) {
                auto pos = transform.GetLocalPosition();
                transform.SetLocalPosition(pos + Vector3(x, y, z));
            },
            
            // Rotation (Euler angles in degrees)
            "getRotation", [&transform]() {
                auto rot = transform.GetLocalRotation();
                // Quaternionからオイラー角に変換（簡易版）
                float x = rot.GetX(), y = rot.GetY(), z = rot.GetZ(), w = rot.GetW();
                float pitch = std::asin(2.0f * (w * y - z * x));
                float yaw = std::atan2(2.0f * (w * z + x * y), 1.0f - 2.0f * (y * y + z * z));
                float roll = std::atan2(2.0f * (w * x + y * z), 1.0f - 2.0f * (x * x + y * y));
                // ラジアンから度に変換
                constexpr float toDeg = 57.2957795f;
                return std::make_tuple(roll * toDeg, pitch * toDeg, yaw * toDeg);
            },
            "setRotation", [&transform](float roll, float pitch, float yaw) {
                // 度からラジアンに変換
                constexpr float toRad = 0.0174532925f;
                auto rot = Quaternion::RotationRollPitchYaw(pitch * toRad, yaw * toRad, roll * toRad);
                transform.SetLocalRotation(rot);
            },
            
            // Scale
            "getScale", [&transform]() {
                auto scale = transform.GetLocalScale();
                return std::make_tuple(scale.GetX(), scale.GetY(), scale.GetZ());
            },
            "setScale", [&transform](float x, float y, float z) {
                transform.SetLocalScale(Vector3(x, y, z));
            }
        );
    }

    // ===== Time =====
    lua["Time"] = lua.create_table_with(
        "deltaTime", 0.0f  // 毎フレーム更新される
    );

    // ===== Debug =====
    lua["Debug"] = lua.create_table_with(
        "log", [](const std::string& msg) { Logger::Info("[Lua] {}", msg); },
        "warn", [](const std::string& msg) { Logger::Warning("[Lua] {}", msg); },
        "error", [](const std::string& msg) { Logger::Error("[Lua] {}", msg); }
    );

    // ===== Vector3 コンストラクタ =====
    lua["Vector3"] = lua.create_table_with(
        "new", [](float x, float y, float z) {
            return std::make_tuple(x, y, z);
        },
        "zero", []() { return std::make_tuple(0.0f, 0.0f, 0.0f); },
        "one", []() { return std::make_tuple(1.0f, 1.0f, 1.0f); },
        "up", []() { return std::make_tuple(0.0f, 1.0f, 0.0f); },
        "forward", []() { return std::make_tuple(0.0f, 0.0f, 1.0f); },
        "right", []() { return std::make_tuple(1.0f, 0.0f, 0.0f); }
    );

    Logger::Debug("[LuaScriptComponent] Engine API bound to Lua");
}

} // namespace UnoEngine
