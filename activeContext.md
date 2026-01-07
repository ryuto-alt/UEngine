# UnoEngine - Active Context

## Current Branch
`editor`

## Recent Changes (Latest First)
1. **Recast NavMesh** - ベイク機能と塗りつぶし表示を実装
2. **AABB Collision** - コリジョン正常取得
3. **First-Person View** - ターゲットモデル非表示切替
4. **GPU Mipmap** - GPU側でのミップマップ生成に仕様変更

## Active Tasks
- [ ] NavAgentComponent実装中 (新規ファイル作成済み)
- [ ] EditorUI拡張 (インスペクタータブシステム)

## Working Files
- `Engine/Navigation/NavAgentComponent.h/.cpp` - 新規
- `Game/UI/EditorUI.cpp/.h` - 編集中
- `Engine/Graphics/DebugLinePipeline.cpp` - 編集中

## Notes
- NavMeshManager: Recast/Detourによるビルド・クエリ・デバッグ描画
- EditorUI: NavMeshタブ追加、オブジェクトタブとの自動切り替え

## Pending Issues
(なし)

---
*Last Updated: 2026-01-07*
