#pragma once

#include "../Core/GameObject.h"
#include "../Core/Scene.h"
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace UnoEngine {

/// シーンをJSON形式でシリアライズ/デシリアライズするクラス
class SceneSerializer {
public:
    /// シーン全体をJSONファイルに保存
    /// @param gameObjects シーン内のGameObjectのリスト
    /// @param filepath 保存先のJSONファイルパス
    /// @return 保存が成功したかどうか
    static bool SaveScene(const std::vector<std::unique_ptr<GameObject>>& gameObjects, const std::string& filepath);

    /// JSONファイルからシーンをロード
    /// @param filepath ロードするJSONファイルパス
    /// @param outGameObjects ロードしたGameObjectの格納先
    /// @return ロードが成功したかどうか
    static bool LoadScene(const std::string& filepath, std::vector<std::unique_ptr<GameObject>>& outGameObjects);

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
