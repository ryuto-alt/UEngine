# シーンシリアライゼーション機能

## 概要

UnoEngineのエディタUIにシーンの保存/ロード機能を追加しました。この機能により、エディタでモデルを配置して**Ctrl+S**で保存すると、次回起動時に自動的にシーンが復元されます。

## 使い方

### 1. シーンの編集

1. エディタを起動します
2. **Hierarchy**パネルでGameObjectを選択
3. **Scene View**でギズモを使ってオブジェクトを移動/回転/スケール
4. または、**Inspector**でTransformの値を編集

### 2. シーンの保存

- **Ctrl+S**を押すと、現在のシーンが `assets/scenes/default_scene.json` に保存されます
- コンソールに保存成功のメッセージが表示されます

### 3. シーンの自動ロード

- 次回エディタ起動時に、保存されたシーンファイルが存在する場合は自動的にロードされます
- シーンファイルが存在しない場合は、デフォルトシーンが作成されます

## 保存されるデータ

現在、以下のデータが保存されます:

- **GameObject**
  - 名前
  - アクティブ状態
  - レイヤー
  - Transform (Position, Rotation, Scale)

- **Components**
  - `SkinnedMeshRenderer`: モデルパス
  - `AnimatorComponent`: 自動的に復元 (SkinnedMeshRendererから)

## JSON フォーマット例

```json
{
    "scene_name": "Scene",
    "version": "1.0",
    "objects": [
        {
            "name": "walk",
            "active": true,
            "layer": 1,
            "transform": {
                "position": [0.0, 0.0, 0.0],
                "rotation": [0.0, 0.0, 0.0, 1.0],
                "scale": [1.0, 1.0, 1.0]
            },
            "components": [
                {
                    "type": "SkinnedMeshRenderer",
                    "modelPath": "assets/model/testmodel/walk.gltf"
                }
            ]
        }
    ]
}
```

## 実装詳細

### SceneSerializer クラス

`Engine/Scene/SceneSerializer.h` および `.cpp` で実装されています。

- `SaveScene()`: シーンをJSONファイルに保存
- `LoadScene()`: JSONファイルからシーンをロード
- `nlohmann::json`ライブラリを使用

### EditorUI 統合

- `EditorUI::SaveScene()`: シーン保存関数
- `EditorUI::LoadScene()`: シーンロード関数
- `ProcessHotkeys()`: Ctrl+S で SaveScene() を呼び出し

### GameScene 統合

- `OnLoad()`: 起動時にシーンファイルの存在をチェック
- 存在する場合は `SceneSerializer::LoadScene()` でロード
- 存在しない場合はデフォルトシーンを作成

## 今後の拡張

以下のコンポーネントやデータの保存に対応予定:

- DirectionalLightComponent
- カスタムコンポーネント
- カメラ設定
- マテリアル設定
- プレハブシステム
