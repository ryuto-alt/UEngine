# UnoEngine アーキテクチャ刷新計画書 (System-Oriented Design)

## 1. 設計思想：脱・Godクラス
従来の「Sceneクラスになんでも書く」スタイルを廃止し、**「Sceneはデータとシステムを保持するコンテナ」**という役割に徹します。これにより、機能追加（新しい敵、ギミック、UIなど）がSceneやApplicationを汚染せず、独立した「System」を追加するだけで完結するようになります。

### 主な変更点
1.  **GameScene**: 具体的なロジックを持たず、`GameObject` のリストと `ISystem` のリストを管理するだけにする。
2.  **ISystem**: 全てのロジック（移動、描画、AIなど）の共通インターフェース。
3.  **GameApplication**: ゲーム固有のシステム（PlayerSystemなど）への直接依存を排除する。

---

## 2. ディレクトリ構造案

```text
UnoEngine/
├── Engine/
│   ├── Core/
│   │   ├── ISystem.h          <-- NEW: システム基底インターフェース
│   │   ├── GameObject.h
│   │   └── Component.h
│   ├── Scene/
│   │   ├── GameScene.h        <-- UPDATED: システム管理機能を追加
│   │   └── GameScene.cpp
│   └── Graphics/
│       └── ...
└── Game/                      <-- ゲーム固有のコードはここに集約
    ├── Scenes/
    │   └── Level1Scene.h      <-- 具体的なステージ設定
    └── Systems/               <-- ロジックは全てここ
        ├── PlayerSystem.h
        ├── RenderSystem.h
        └── PhysicsSystem.h
```

---

## 3. コア・アーキテクチャ実装 (Engine側)

### 3.1. ISystem (システム基底)
全てのロジックの親となるインターフェースです。

```cpp
// Engine/Core/ISystem.h
#pragma once

namespace UnoEngine {

class GameScene;

class ISystem {
public:
    virtual ~ISystem() = default;

    // シーン開始時の初期化（リソースロードなど）
    virtual void OnInitialize(GameScene* scene) {}

    // 毎フレームの更新処理（ゲームロジック）
    virtual void OnUpdate(GameScene* scene, float deltaTime) {}

    // 描画コマンドの発行
    virtual void OnRender(GameScene* scene) {}
    
    // シーン終了時の後始末
    virtual void OnShutdown(GameScene* scene) {}
};

}
```

### 3.2. GameScene (コンテナ化)
ロジックを直接書くのではなく、登録されたシステムを回す「実行環境」になります。

```cpp
// Engine/Scene/GameScene.h
#pragma once
#include <vector>
#include <memory>
#include <string>
#include "Engine/Core/ISystem.h"
#include "Engine/Core/GameObject.h"

namespace UnoEngine {

class GameApplication; // 前方宣言

class GameScene {
public:
    GameScene(GameApplication* app) : application_(app) {}
    virtual ~GameScene() { Shutdown(); }

    // --- ライフサイクル ---
    void Initialize() {
        OnLoad(); // 派生クラスでのセットアップ（システムの登録など）
        for (auto& sys : systems_) sys->OnInitialize(this);
    }

    void Update(float deltaTime) {
        for (auto& sys : systems_) sys->OnUpdate(this, deltaTime);
        OnUpdateInternal(deltaTime);
    }

    void Render() {
        for (auto& sys : systems_) sys->OnRender(this);
    }

    void Shutdown() {
        for (auto& sys : systems_) sys->OnShutdown(this);
        systems_.clear();
        gameObjects_.clear();
    }

    // --- 登録・管理メソッド ---
    
    // システムの登録 (Template)
    template <typename T, typename... Args>
    T* AddSystem(Args&&... args) {
        auto sys = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = sys.get();
        systems_.push_back(std::move(sys));
        return ptr;
    }

    // ゲームオブジェクト作成
    GameObject* CreateGameObject(const std::string& name) {
        auto go = std::make_unique<GameObject>(name);
        GameObject* ptr = go.get();
        gameObjects_.push_back(std::move(go));
        return ptr;
    }

    // アクセサ
    GameApplication* GetApplication() const { return application_; }
    const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const { return gameObjects_; }
    
    // 簡易的なカメラ取得（本来はCameraSystemが管理すべきだが過渡期として用意）
    void SetActiveCamera(Camera* cam) { activeCamera_ = cam; }
    Camera* GetActiveCamera() const { return activeCamera_; }

protected:
    // ユーザーが実装するのはこれだけ
    virtual void OnLoad() = 0;
    virtual void OnUpdateInternal(float dt) {}

private:
    GameApplication* application_ = nullptr;
    Camera* activeCamera_ = nullptr;
    
    std::vector<std::unique_ptr<ISystem>> systems_;
    std::vector<std::unique_ptr<GameObject>> gameObjects_;
};

}
```

---

## 4. システム実装例 (Game側)

### 4.1. RenderSystem (描画ロジックの分離)
`GameScene::OnRender` にあった処理を独立させます。

```cpp
// Game/Systems/RenderSystem.h
#pragma once
#include "Engine/Core/ISystem.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Scene/GameScene.h"
#include "Engine/Components/MeshRenderer.h"

namespace UnoEngine {

class RenderSystem : public ISystem {
public:
    RenderSystem(Renderer* renderer) : renderer_(renderer) {}

    void OnRender(GameScene* scene) override {
        Camera* camera = scene->GetActiveCamera();
        if (!camera) return;

        // 1. 描画対象(RenderItem)の収集
        std::vector<RenderItem> items;
        for (const auto& go : scene->GetGameObjects()) {
            // 非アクティブならスキップなどの判定もここで可能
            if (!go->IsActive()) continue;

            auto* meshRenderer = go->GetComponent<MeshRenderer>();
            if (meshRenderer) {
                RenderItem item;
                item.mesh = meshRenderer->GetMesh();
                item.material = meshRenderer->GetMaterial();
                item.worldMatrix = go->GetTransform().GetWorldMatrix();
                items.push_back(item);
            }
        }

        // 2. ライティング情報の収集 (DirectionalLightComponentを探す)
        LightManager lights;
        // ... (ループしてLightコンポーネントを探し、lightsに追加する処理) ...

        // 3. 描画実行
        RenderView view;
        view.camera = camera;
        renderer_->Draw(view, items, &lights, scene);
    }

private:
    Renderer* renderer_;
};

}
```

### 4.2. PlayerSystem (操作ロジックの分離)
`GameApplication` から依存を外し、独立して動くようにします。

```cpp
// Game/Systems/PlayerSystem.h
#pragma once
#include "Engine/Core/ISystem.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Scene/GameScene.h"
#include "Game/Components/Player.h"

namespace UnoEngine {

class PlayerSystem : public ISystem {
public:
    void OnUpdate(GameScene* scene, float deltaTime) override {
        // 必要な外部システムを取得
        // (InputManagerはApplicationかSceneが持っている想定)
        auto* input = scene->GetApplication()->GetInputManager(); 
        auto* camera = scene->GetActiveCamera();

        if (!input || !camera) return;

        // Playerコンポーネントを持つオブジェクトを探して動かす
        for (const auto& go : scene->GetGameObjects()) {
            auto* player = go->GetComponent<Player>();
            if (player) {
                UpdatePlayer(go->GetTransform(), player, input, camera, deltaTime);
            }
        }
    }

private:
    void UpdatePlayer(Transform& transform, Player* player, InputManager* input, Camera* camera, float dt) {
        // 具体的な移動ロジック (WASDなど)
        // ...
    }
};

}
```

---

## 5. 実際の使用方法 (ユーザーコード)

ここまで準備ができれば、ゲームのシーン作成は非常にシンプルかつ宣言的になります。

```cpp
// Game/Scenes/MainGameScene.h
#pragma once
#include "Engine/Scene/GameScene.h"
#include "Game/Systems/PlayerSystem.h"
#include "Game/Systems/RenderSystem.h"

namespace UnoEngine {

class MainGameScene : public GameScene {
public:
    using GameScene::GameScene; // コンストラクタ継承

protected:
    void OnLoad() override {
        // ---------------------------------------------------
        // 1. システムの構築 (このシーンのルールを定義)
        // ---------------------------------------------------
        // プレイヤー操作を有効化
        AddSystem<PlayerSystem>();
        
        // 描画を有効化 (RendererはApplicationから借りる)
        AddSystem<RenderSystem>(GetApplication()->GetRenderer());


        // ---------------------------------------------------
        // 2. ワールドの構築 (オブジェクトの配置)
        // ---------------------------------------------------
        // プレイヤー作成
        auto* playerGO = CreateGameObject("Hero");
        playerGO->AddComponent<Player>();
        auto* mesh = GetApplication()->LoadMesh("hero.obj");
        playerGO->AddComponent<MeshRenderer>(mesh, mesh->GetMaterial());

        // カメラ作成
        auto* cameraGO = CreateGameObject("MainCam");
        auto* camComp = cameraGO->AddComponent<Camera>();
        SetActiveCamera(camComp); // 暫定的なカメラ設定

        // ライト作成
        auto* lightGO = CreateGameObject("Sun");
        lightGO->AddComponent<DirectionalLightComponent>();
    }
};

}
```

## 6. 今後の拡張ステップ

この基盤ができたら、次は以下のような機能拡張が容易になります。

* **PhysicsSystem (物理演算)**: `AddSystem<PhysicsSystem>()` するだけで、Rigidbodyを持つ全オブジェクトが物理挙動を開始する。
* **UISystem (ユーザーインターフェース)**: ImGuiやゲーム内UIの描画を一手に引き受けるシステム。
* **EventBus (イベント駆動)**: `PlayerSystem` が `PlayerJumpEvent` を発行し、`AudioSystem` がそれを検知して音を鳴らすような、疎結合な通信の実装。