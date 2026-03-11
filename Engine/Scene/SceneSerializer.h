#pragma once

#include "../Core/GameObject.h"
#include "../Core/Scene.h"
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace UnoEngine {

class GrassSystem;

/// シーンをJSON形式でシリアライズ/デシリアライズするクラス
class SceneSerializer {
public:
    /// シーン全体をJSONファイルに保存
    /// @param gameObjects シーン内のGameObjectのリスト
    /// @param filepath 保存先のJSONファイルパス
    /// @param grassSystem 草原システム（nullptrの場合は草データを保存しない）
    /// @return 保存が成功したかどうか
    static bool SaveScene(const std::vector<std::unique_ptr<GameObject>>& gameObjects, const std::string& filepath, GrassSystem* grassSystem = nullptr);

    /// JSONファイルからシーンをロード
    /// @param filepath ロードするJSONファイルパス
    /// @param outGameObjects ロードしたGameObjectの格納先
    /// @param grassSystem 草原システム（nullptrの場合は草データをロードしない）
    /// @return ロードが成功したかどうか
    static bool LoadScene(const std::string& filepath, std::vector<std::unique_ptr<GameObject>>& outGameObjects, GrassSystem* grassSystem = nullptr);

    /// GameObjectをJSON文字列にシリアライズ（Prefab用）
    static std::string SerializeSingleObject(const GameObject& obj);

    /// JSON文字列からGameObjectを復元（Prefab用）
    static std::unique_ptr<GameObject> DeserializeSingleObject(const std::string& jsonStr);

    /// イントロシネマティックパス（Save/Loadで自動的に読み書きされる）
    static inline std::string s_introCinematicPath;

private:
    /// GameObject単体をJSONにシリアライズ
    static nlohmann::json SerializeGameObject(const GameObject& gameObject);

    /// JSONからGameObjectを復元
    static std::unique_ptr<GameObject> DeserializeGameObject(const nlohmann::json& json);

    /// Transform情報をJSONにシリアライズ
    static nlohmann::json SerializeTransform(const Transform& transform);

    /// JSONからTransform情報を復元
    static void DeserializeTransform(const nlohmann::json& json, Transform& transform);

    /// Component情報をJSONにシリアライズ
    static nlohmann::json SerializeComponent(const Component& component);

    /// JSONからComponent情報を復元（コンポーネントタイプに応じて生成）
    static void DeserializeComponent(const nlohmann::json& json, GameObject& gameObject);
};

} // namespace UnoEngine
