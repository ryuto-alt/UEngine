# RenderView システム完全ガイド

## 📚 目次
1. [基本概念](#基本概念)
2. [データフロー](#データフロー)
3. [実装例](#実装例)
4. [応用パターン](#応用パターン)
5. [ビット演算の仕組み](#ビット演算の仕組み)

---

## 基本概念

### RenderViewとは？

**RenderViewは「どのように描画するか」の設定を入れる箱**です。

```cpp
struct RenderView {
    Camera* camera = nullptr;           // 視点（どこから見るか）
    uint32 layerMask = 0xFFFFFFFF;     // フィルター（何を描画するか）
    std::string viewName = "Main";      // 名前（デバッグ用）
};
```

### なぜ参照渡し（&）を使うのか？

```cpp
// ❌ 値渡し（コピー）
void OnRender(RenderView view) {
    view.camera = camera;  // コピーを変更しても外に影響なし
}

// ✅ 参照渡し（直接変更）
void OnRender(RenderView& view) {
    view.camera = camera;  // 外のviewが直接書き換わる！
}
```

**メリット:**
- コピーのコストがない（高速）
- 呼び出し元のデータを直接変更できる
- 複数の設定を一度に返せる

---

## データフロー

### 全体の流れ

```
┌─────────────────────────────────────────────────────────────┐
│ 1. Application::OnRender()                                  │
│    RenderView view;  // 空の箱を作成                         │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. Scene::OnRender(RenderView& view)                        │
│    view.camera = GetActiveCamera();   // カメラ設定          │
│    view.layerMask = DEFAULT | PLAYER; // フィルター設定      │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. RenderSystem::CollectRenderables(scene, view)            │
│    for (auto& obj : scene->GetGameObjects()) {              │
│        if (PassesLayerMask(obj->GetLayer(), view.layerMask))│
│            items.push_back(obj);  // フィルタリング           │
│    }                                                         │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ 4. Renderer::Draw(view, items, lights)                      │
│    viewMatrix = view.camera->GetViewMatrix();  // カメラ使用 │
│    projection = view.camera->GetProjectionMatrix();         │
└─────────────────────────────────────────────────────────────┘
```

### コード例

```cpp
// Application.cpp
void Application::OnRender() {
    // 【ステップ1】空のRenderViewを作成
    RenderView view;

    // 【ステップ2】Sceneに設定してもらう（参照渡し）
    if (activeScene) {
        activeScene->OnRender(view);  // viewの中身が書き換わる！
    }

    // 【ステップ3】設定済みのviewを使ってフィルタリング
    auto items = renderSystem_->CollectRenderables(activeScene, view);

    // 【ステップ4】描画
    renderer_->Draw(view, items, lightManager_.get());
}
```

---

## 実装例

### 例1: 基本的なゲームシーン

```cpp
class GameScene : public Scene {
public:
    void OnRender(RenderView& view) override {
        Camera* camera = GetActiveCamera();
        if (!camera) return;

        // カメラ設定
        view.camera = camera;

        // ゲームオブジェクトとプレイヤーのみ描画（UIは除外）
        view.layerMask = Layers::DEFAULT | Layers::PLAYER | Layers::ENEMY;

        view.viewName = "MainView";
    }
};
```

**結果:**
- DEFAULT, PLAYER, ENEMYレイヤーのオブジェクトが描画される
- UIレイヤーのオブジェクトは描画されない

### 例2: UI専用シーン

```cpp
class MenuScene : public Scene {w
public:
    void OnRender(RenderView& view) override {
        view.camera = uiCamera_;  // 正射影カメラ
        view.layerMask = Layers::UI;  // UIだけ
        view.viewName = "UIView";
    }
};
```

**結果:**
- UIレイヤーのオブジェクトのみ描画される
- ゲームオブジェクトは全て無視される

### 例3: デバッグモード

```cpp
class GameScene : public Scene {
public:
    void OnRender(RenderView& view) override {
        view.camera = GetActiveCamera();

        if (debugMode) {
            // デバッグ時は全て描画
            view.layerMask = 0xFFFFFFFF;
            view.viewName = "DebugView";
        } else {
            // 通常時はUI以外
            view.layerMask = ~Layers::UI;
            view.viewName = "GameView";
        }
    }
};
```

---

## 応用パターン

### パターン1: 2カメラシステム（Split Screen）

```cpp
class SplitScreenScene : public Scene {
private:
    Camera* player1Camera_;
    Camera* player2Camera_;

public:
    void OnRender(RenderView& view) override {
        // この場合、Application側で複数回呼ぶ必要がある
        // ここでは1つ目のビューのみ設定
        view.camera = player1Camera_;
        view.layerMask = Layers::DEFAULT | Layers::PLAYER;
        view.viewName = "Player1View";
    }

    void OnRenderSecondView(RenderView& view) {
        view.camera = player2Camera_;
        view.layerMask = Layers::DEFAULT | Layers::PLAYER;
        view.viewName = "Player2View";
    }
};
```

```cpp
// Application.cpp（カスタマイズ版）
void Application::OnRender() {
    auto* splitScene = dynamic_cast<SplitScreenScene*>(activeScene);

    if (splitScene) {
        // プレイヤー1用
        RenderView view1;
        splitScene->OnRender(view1);
        SetViewport(0, 0, width/2, height);  // 左半分
        auto items1 = renderSystem_->CollectRenderables(splitScene, view1);
        renderer_->Draw(view1, items1, lightManager_.get());

        // プレイヤー2用
        RenderView view2;
        splitScene->OnRenderSecondView(view2);
        SetViewport(width/2, 0, width/2, height);  // 右半分
        auto items2 = renderSystem_->CollectRenderables(splitScene, view2);
        renderer_->Draw(view2, items2, lightManager_.get());
    }
}
```

### パターン2: ミニマップ

```cpp
class GameScene : public Scene {
private:
    Camera* mainCamera_;
    Camera* minimapCamera_;  // 上から見下ろすカメラ

public:
    void OnRender(RenderView& view) override {
        // メイン画面
        view.camera = mainCamera_;
        view.layerMask = Layers::DEFAULT | Layers::PLAYER | Layers::ENEMY;
        view.viewName = "MainView";
    }

    void OnRenderMinimap(RenderView& view) {
        // ミニマップ（UIは表示しない）
        view.camera = minimapCamera_;
        view.layerMask = Layers::PLAYER | Layers::ENEMY;  // DEFAULTは重いので除外
        view.viewName = "MinimapView";
    }
};
```

### パターン3: カメラエフェクト切り替え

```cpp
class GameScene : public Scene {
private:
    Camera* normalCamera_;
    Camera* nightVisionCamera_;  // 緑フィルター
    Camera* thermalCamera_;      // 赤外線
    bool nightVision_ = false;

public:
    void OnRender(RenderView& view) override {
        // モードによってカメラ切り替え
        if (nightVision_) {
            view.camera = nightVisionCamera_;
            view.layerMask = Layers::DEFAULT | Layers::ENEMY;  // プレイヤーは見えない
        } else {
            view.camera = normalCamera_;
            view.layerMask = Layers::DEFAULT | Layers::PLAYER | Layers::ENEMY;
        }
        view.viewName = nightVision_ ? "NightVision" : "Normal";
    }
};
```

### パターン4: レイヤーの動的変更

```cpp
class StealthGameScene : public Scene {
private:
    bool discovered_ = false;  // プレイヤーが発見されたか

public:
    void OnRender(RenderView& view) override {
        view.camera = GetActiveCamera();

        if (discovered_) {
            // 発見後: 敵の視線も表示
            view.layerMask = Layers::DEFAULT | Layers::PLAYER |
                           Layers::ENEMY | Layers::DEBUG;
        } else {
            // 潜入中: 敵の視線は非表示
            view.layerMask = Layers::DEFAULT | Layers::PLAYER | Layers::ENEMY;
        }

        view.viewName = discovered_ ? "AlertMode" : "StealthMode";
    }
};
```

---

## ビット演算の仕組み

### レイヤーの定義

```cpp
namespace Layers {
    using Layer = uint32;

    constexpr Layer DEFAULT = 1 << 0;  // 0b00000001 = 1
    constexpr Layer PLAYER  = 1 << 1;  // 0b00000010 = 2
    constexpr Layer ENEMY   = 1 << 2;  // 0b00000100 = 4
    constexpr Layer UI      = 1 << 3;  // 0b00001000 = 8
}
```

### ビット演算子

| 演算子 | 意味 | 例 |
|--------|------|-----|
| `\|` (OR) | ビットを立てる | `DEFAULT \| PLAYER` |
| `&` (AND) | 両方立っているか | `layer & mask` |
| `~` (NOT) | ビット反転 | `~UI` (UI以外) |
| `^` (XOR) | 排他的論理和 | あまり使わない |

### 組み合わせの例

```cpp
// 【例1】複数レイヤーを有効化
uint32 mask = Layers::DEFAULT | Layers::PLAYER | Layers::ENEMY;

// 2進数で見ると:
// DEFAULT = 0b00000001
// PLAYER  = 0b00000010
// ENEMY   = 0b00000100
// ───────────────────
// mask    = 0b00000111  (ビットが3つ立っている)
```

```cpp
// 【例2】フィルタリングチェック
bool PassesLayerMask(uint32 objectLayer, uint32 viewMask) {
    return (objectLayer & viewMask) != 0;
}

// ケース1: PLAYERオブジェクトをチェック
objectLayer = Layers::PLAYER;     // 0b00000010
viewMask = DEFAULT | PLAYER;      // 0b00000011
結果 = 0b00000010 & 0b00000011 = 0b00000010 (≠ 0) → true (描画する)

// ケース2: UIオブジェクトをチェック
objectLayer = Layers::UI;         // 0b00001000
viewMask = DEFAULT | PLAYER;      // 0b00000011
結果 = 0b00001000 & 0b00000011 = 0b00000000 (= 0) → false (描画しない)
```

```cpp
// 【例3】UI以外を描画
uint32 mask = ~Layers::UI;

// 2進数で見ると:
// UI   = 0b00001000
// ~UI  = 0b11110111  (UIビットだけが0、他は全て1)
```

### 実用例: カスタムレイヤー

```cpp
namespace Layers {
    constexpr Layer DEFAULT      = 1 << 0;   // 0b00000001
    constexpr Layer PLAYER       = 1 << 1;   // 0b00000010
    constexpr Layer ENEMY        = 1 << 2;   // 0b00000100
    constexpr Layer UI           = 1 << 3;   // 0b00001000
    constexpr Layer PARTICLE     = 1 << 4;   // 0b00010000
    constexpr Layer TRANSPARENT  = 1 << 5;   // 0b00100000
    constexpr Layer DEBUG        = 1 << 6;   // 0b01000000
    constexpr Layer MINIMAP_ONLY = 1 << 7;   // 0b10000000
}

// メインビュー: パーティクル以外
view.layerMask = ~Layers::PARTICLE;

// ミニマップビュー: 特定のもののみ
view.layerMask = Layers::PLAYER | Layers::ENEMY | Layers::MINIMAP_ONLY;

// デバッグビュー: 全て
view.layerMask = 0xFFFFFFFF;

// 透明オブジェクト専用パス
view.layerMask = Layers::TRANSPARENT;
```

---

## まとめ

### RenderViewシステムの利点

1. **柔軟性**
   - シーンごとに異なる描画設定
   - 動的にレイヤーを変更可能

2. **パフォーマンス**
   - ビット演算で高速フィルタリング
   - 不要なオブジェクトを早期に除外

3. **拡張性**
   - 複数カメラ対応が容易
   - ミニマップ、スプリットスクリーン等

4. **デバッグ性**
   - viewNameでプロファイリング
   - レイヤーマスクで表示切り替え

### 典型的な使用パターン

```cpp
// パターン1: シンプル
view.camera = mainCamera;
view.layerMask = Layers::DEFAULT | Layers::PLAYER;

// パターン2: UI除外
view.layerMask = ~Layers::UI;

// パターン3: 特定レイヤーのみ
view.layerMask = Layers::ENEMY;

// パターン4: 全て
view.layerMask = 0xFFFFFFFF;

// パターン5: 条件分岐
view.layerMask = debugMode ? 0xFFFFFFFF : (Layers::DEFAULT | Layers::PLAYER);
```

### 将来の拡張可能性

- **マルチパスレンダリング**: Shadow Pass, Main Pass, Post Process
- **VRレンダリング**: 左目/右目で異なるview
- **リフレクション**: 鏡面反射用の別カメラ
- **ポータル**: ポータル越しの視点

この設計により、**1つのRenderViewで様々なレンダリングシナリオに対応**できます！
