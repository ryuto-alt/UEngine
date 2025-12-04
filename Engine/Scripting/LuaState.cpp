#include "pch.h"
#include "LuaState.h"
#include <fstream>
#include <sstream>
#include <regex>

namespace UnoEngine {

LuaState::LuaState() = default;

LuaState::~LuaState() = default;

bool LuaState::Initialize() {
    if (isInitialized_) {
        return true;
    }

    try {
        lua_ = std::make_unique<sol::state>();
        
        // 標準ライブラリを開く
        lua_->open_libraries(
            sol::lib::base,
            sol::lib::package,
            sol::lib::string,
            sol::lib::math,
            sol::lib::table,
            sol::lib::os,
            sol::lib::io
        );

        // print関数をLogger経由に置き換え
        lua_->set_function("print", [](sol::variadic_args va) {
            std::ostringstream oss;
            bool first = true;
            for (auto arg : va) {
                if (!first) oss << "\t";
                first = false;
                
                sol::type t = arg.get_type();
                switch (t) {
                    case sol::type::string:
                        oss << arg.as<std::string>();
                        break;
                    case sol::type::number:
                        oss << arg.as<double>();
                        break;
                    case sol::type::boolean:
                        oss << (arg.as<bool>() ? "true" : "false");
                        break;
                    case sol::type::nil:
                        oss << "nil";
                        break;
                    default:
                        oss << "[" << sol::type_name(arg.lua_state(), t) << "]";
                        break;
                }
            }
            Logger::Info("[Lua] {}", oss.str());
        });

        isInitialized_ = true;
        Logger::Info("[LuaState] Lua initialized successfully");
        return true;

    } catch (const sol::error& e) {
        HandleError(e);
        return false;
    }
}

bool LuaState::LoadScript(std::string_view scriptPath) {
    if (!isInitialized_) {
        HandleError("LuaState not initialized");
        return false;
    }

    scriptPath_ = std::string(scriptPath);

    // ファイルの存在確認
    if (!std::filesystem::exists(scriptPath_)) {
        HandleError(std::string_view("Script file not found: " + scriptPath_));
        return false;
    }

    try {
        // ファイル読み込み・実行
        auto result = lua_->safe_script_file(scriptPath_, sol::script_pass_on_error);
        
        if (!result.valid()) {
            sol::error err = result;
            HandleError(err);
            return false;
        }

        // 最終更新時刻を記録
        lastModifiedTime_ = std::filesystem::last_write_time(scriptPath_);

        Logger::Info("[LuaState] Script loaded: {}", scriptPath_);
        return true;

    } catch (const sol::error& e) {
        HandleError(e);
        return false;
    }
}

bool LuaState::ExecuteString(std::string_view code) {
    if (!isInitialized_) {
        HandleError("LuaState not initialized");
        return false;
    }

    try {
        auto result = lua_->safe_script(code, sol::script_pass_on_error);
        
        if (!result.valid()) {
            sol::error err = result;
            HandleError(err);
            return false;
        }

        return true;

    } catch (const sol::error& e) {
        HandleError(e);
        return false;
    }
}

void LuaState::CallAwake() {
    SafeCall("Awake");
}

void LuaState::CallStart() {
    SafeCall("Start");
}

void LuaState::CallUpdate(float deltaTime) {
    SafeCall("Update", deltaTime);
}

void LuaState::CallOnDestroy() {
    SafeCall("OnDestroy");
}

std::vector<ScriptProperty> LuaState::GetPublicProperties() const {
    std::vector<ScriptProperty> properties;

    if (!isInitialized_ || !lua_) {
        return properties;
    }

    try {
        // グローバルテーブルを走査してプロパティを収集
        sol::table globals = lua_->globals();
        
        for (auto& [key, value] : globals) {
            // キーが文字列でない場合はスキップ
            if (key.get_type() != sol::type::string) continue;
            
            std::string name = key.as<std::string>();
            
            // アンダースコアで始まるものはプライベートとみなす
            if (name.empty() || name[0] == '_') continue;
            
            // 関数、テーブル、userdata等はスキップ
            sol::type t = value.get_type();
            
            ScriptProperty prop;
            prop.name = name;
            
            switch (t) {
                case sol::type::boolean:
                    prop.value = value.as<bool>();
                    prop.defaultValue = prop.value;
                    properties.push_back(std::move(prop));
                    break;
                    
                case sol::type::number:
                    // 整数か浮動小数点かを判定
                    {
                        double num = value.as<double>();
                        if (num == static_cast<int32>(num)) {
                            prop.value = static_cast<int32>(num);
                        } else {
                            prop.value = static_cast<float>(num);
                        }
                        prop.defaultValue = prop.value;
                        properties.push_back(std::move(prop));
                    }
                    break;
                    
                case sol::type::string:
                    prop.value = value.as<std::string>();
                    prop.defaultValue = prop.value;
                    properties.push_back(std::move(prop));
                    break;
                    
                default:
                    // その他の型は無視
                    break;
            }
        }

    } catch (const sol::error& e) {
        Logger::Warning("[LuaState] Error getting properties: {}", e.what());
    }

    return properties;
}

void LuaState::SetProperty(std::string_view name, const ScriptPropertyValue& value) {
    if (!isInitialized_ || !lua_) return;

    try {
        std::visit([&](auto&& val) {
            (*lua_)[std::string(name)] = val;
        }, value);

    } catch (const sol::error& e) {
        Logger::Warning("[LuaState] Error setting property '{}': {}", name, e.what());
    }
}

std::optional<ScriptPropertyValue> LuaState::GetProperty(std::string_view name) const {
    if (!isInitialized_ || !lua_) return std::nullopt;

    try {
        sol::object obj = (*lua_)[std::string(name)];
        
        switch (obj.get_type()) {
            case sol::type::boolean:
                return obj.as<bool>();
            case sol::type::number: {
                double num = obj.as<double>();
                if (num == static_cast<int32>(num)) {
                    return static_cast<int32>(num);
                }
                return static_cast<float>(num);
            }
            case sol::type::string:
                return obj.as<std::string>();
            default:
                return std::nullopt;
        }

    } catch (const sol::error& e) {
        Logger::Warning("[LuaState] Error getting property '{}': {}", name, e.what());
        return std::nullopt;
    }
}

std::filesystem::file_time_type LuaState::GetLastModifiedTime() const {
    if (scriptPath_.empty() || !std::filesystem::exists(scriptPath_)) {
        return {};
    }
    return std::filesystem::last_write_time(scriptPath_);
}

bool LuaState::CheckAndReload() {
    if (scriptPath_.empty()) return false;

    auto currentTime = GetLastModifiedTime();
    if (currentTime > lastModifiedTime_) {
        Logger::Info("[LuaState] Script modified, reloading: {}", scriptPath_);
        
        // スクリプトを再読み込み
        std::string path = scriptPath_;
        
        // 現在のプロパティを保存
        auto properties = GetPublicProperties();
        
        // Luaステートを再初期化
        lua_ = std::make_unique<sol::state>();
        isInitialized_ = false;
        
        if (!Initialize()) return false;
        if (!LoadScript(path)) return false;
        
        // プロパティを復元
        for (const auto& prop : properties) {
            SetProperty(prop.name, prop.value);
        }
        
        return true;
    }
    
    return false;
}

void LuaState::HandleError(const sol::error& e) {
    HandleError(std::string_view(e.what()));
}

void LuaState::HandleError(std::string_view message) {
    LuaError error;
    error.message = std::string(message);
    error.scriptPath = scriptPath_;
    
    // 行番号を抽出（形式: "[string \"...\"]:行番号:" または "ファイル名:行番号:"）
    std::regex lineRegex(R"(:(\d+):)");
    std::smatch match;
    std::string msgStr(message);
    if (std::regex_search(msgStr, match, lineRegex)) {
        error.line = std::stoi(match[1].str());
    }
    
    lastError_ = std::move(error);
    
    Logger::Error("[LuaState] Error: {}", message);
}

bool LuaState::HasFunction(std::string_view name) const {
    if (!isInitialized_ || !lua_) return false;
    
    sol::object func = (*lua_)[std::string(name)];
    return func.is<sol::function>();
}

template<typename... Args>
void LuaState::SafeCall(std::string_view funcName, Args&&... args) {
    if (!isInitialized_ || !lua_) return;
    if (!HasFunction(funcName)) return;

    try {
        sol::protected_function func = (*lua_)[std::string(funcName)];
        auto result = func(std::forward<Args>(args)...);
        
        if (!result.valid()) {
            sol::error err = result;
            HandleError(err);
        }

    } catch (const sol::error& e) {
        HandleError(e);
    }
}

// 明示的テンプレートインスタンス化
template void LuaState::SafeCall<>(std::string_view);
template void LuaState::SafeCall<float>(std::string_view, float&&);
template void LuaState::SafeCall<float&>(std::string_view, float&);

} // namespace UnoEngine
