# Tutorial & Event Track System

## 概要

CinematicSequence にイベントトラックを追加し、チュートリアル演出（テキスト表示・音声・オブジェクト操作・入力待ち）を実現するシステム。

## アーキテクチャ

### イベントトラック

CinematicSequence の JSON に `"events"` 配列として統合。カメラキーフレームと同一タイムライン上でイベントを管理する。

```json
{
  "duration": 10.0,
  "keyframes": [ ... ],
  "events": [
    {
      "type": "Text",
      "time": 2.0,
      "duration": 3.0,
      "text": "WASDで移動できます",
      "displayStyle": "Subtitle",
      "fontSize": 1.2,
      "fadeIn": 0.5,
      "fadeOut": 0.5
    },
    {
      "type": "WaitForInput",
      "time": 5.0,
      "waitKey": "Any"
    }
  ]
}
```

### イベントタイプ

| タイプ | 説明 | 主要パラメータ |
|--------|------|----------------|
| **Text** | テキスト表示 | text, displayStyle, fontSize, fadeIn/fadeOut |
| **Audio** | SE/BGM再生 | audioClip, volume |
| **ObjectToggle** | GameObject表示切替 | targetObject, visible |
| **Lua** | Lua関数呼び出し | luaFunction |
| **WaitForInput** | 再生一時停止＋キー入力待ち | waitKey |

### テキスト表示スタイル

| スタイル | 位置 | 用途 |
|----------|------|------|
| **Subtitle** | 画面下部中央 | ナレーション、字幕 |
| **CenterDialog** | 画面中央 | 重要メッセージ |
| **Bubble** | 画面上部寄り | 吹き出し風ヒント |

## フェーズ分割型入力待ち

チュートリアルの典型的なフロー:

```
Phase1: カメラシネマティック再生
  ↓ (WaitForInputEvent)
Phase2: カメラ停止 + 「WASDで移動」表示
  ↓ (Enter/Spaceキー)
Phase3: 次のシネマティック再生
  ↓ (TextEvent, auto)
Phase4: 「マウスで視点操作」表示 (自動進行3秒)
  ↓
Phase5: チュートリアル終了
```

`WaitForInput` イベントに到達すると、`CinematicPlayer` は時間進行を停止。プレイヤーが Enter/Space を押すと `ResolveWaitForInput()` で再開。

## テキスト描画

### BitmapFont (BMFont形式)

- **パーサー**: BMFont テキスト形式 (.fnt) を解析
- **テクスチャ**: ページごとの .png テクスチャアトラス
- **グリフ**: UV座標、オフセット、advance 幅を格納
- **カーニング**: ペアごとの間隔調整
- **UTF-8**: 日本語・英語のマルチバイト文字対応

### TextRenderer (DX12直描画)

- SpritePipeline (既存) を流用
- グリフ単位でクアッドを生成し、NDC座標変換
- ページ別バッチ描画で draw call 最適化
- アンカー指定: TopLeft, TopCenter, Center, BottomCenter, BottomLeft

### ImGuiオーバーレイ (現在のテキスト表示)

- `CinematicManager::RenderTextOverlay()` でImGuiウィンドウとして描画
- フェードイン/アウトはアルファ値で制御
- WaitForInput中は "Press any key..." を点滅表示

## エディタ統合

CinematicEditor のタイムラインにイベントトラック行を追加:

- カメラトラックの下にイベントトラックを表示
- イベントタイプごとに色分け (Text=緑, Audio=橙, Object=紫, Lua=青, WaitForInput=赤)
- クリックで選択、ドラッグで移動、右クリックで削除/追加
- インスペクターでタイプ別パラメータ編集

## ファイル構成

```
Engine/
├── Graphics/
│   ├── BitmapFont.h/.cpp      # BMFont パーサー + テクスチャ管理
│   └── TextRenderer.h/.cpp    # DX12 グリフバッチ描画
├── Cinematic/
│   ├── CinematicSequence.h    # CinematicEvent 構造体 + JSON シリアライズ
│   ├── CinematicPlayer.h/.cpp # イベント評価 + WaitForInput + フェード計算
│   ├── CinematicManager.h/.cpp # テキストオーバーレイ描画
│   └── CinematicEditor.h/.cpp # タイムライン イベントトラック UI
Game/
├── GameApplication.cpp         # WaitForInput 解除処理
└── UI/EditorUI.cpp             # RenderTextOverlay 呼び出し
```

## 今後の拡張予定

- BitmapFont による DX12 直描画テキスト表示への切り替え
- チュートリアル進行セーブ/ロード
- 条件付きイベント (特定キー入力で分岐)
- 多言語ローカライゼーション対応
