-- EnemyWander.lua
-- NavMeshを使って自動徘徊するEnemyスクリプト
-- Dark Deception風の狭い迷路でも動作

-- public変数（Inspectorに表示される）
wanderRadius = 15.0      -- 徘徊範囲（メートル）
moveSpeed = 7.5          -- 移動速度
waitTime = 1.5           -- 到着後の待機時間（秒）
angularSpeed = 720.0     -- 回転速度（度/秒）高速回転
initialYaw = 0.0         -- 初期向き（ラジアン、0=+Z方向）

-- ローカル変数
local initialized = false
local lastState = ""

function Awake()
    Debug.log("EnemyWander: Awake - " .. gameObject.name)
end

function Start()
    Debug.log("EnemyWander: Start")

    -- NavAgentが存在するか確認
    if not NavAgent then
        Debug.error("EnemyWander: NavAgent component not found!")
        return
    end

    -- 初期向きを設定
    NavAgent.setInitialYaw(initialYaw)

    -- NavAgentのパラメータ設定
    NavAgent.setSpeed(moveSpeed)
    NavAgent.setAngularSpeed(angularSpeed)
    NavAgent.setWaitTime(waitTime)
    NavAgent.setStoppingDistance(0.5)

    -- 直進モードは一旦無効（Crowd本来の動作をテスト）
    -- NavAgent.setDirectMoveEnabled(true)

    -- 徘徊開始（スポーン地点周辺）
    NavAgent.startWander(wanderRadius)

    initialized = true
    Debug.log("EnemyWander: Wandering started with radius " .. wanderRadius)
end

function Update(deltaTime)
    if not initialized or not NavAgent then
        return
    end
    
    -- 状態変化をログ出力（デバッグ用）
    local state = NavAgent.getState()
    if state ~= lastState then
        Debug.log("EnemyWander: State changed to " .. state)
        lastState = state
        
        -- アニメーション切り替え
        if Animator then
            if state == "wandering" or state == "moving" then
                Animator.play("Walk", true)
            elseif state == "idle" or state == "arrived" then
                Animator.play("Idle", true)
            end
        end
    end
    
    -- 速度に応じたアニメーション（オプション）
    -- local vx, vy, vz = NavAgent.getVelocity()
    -- local speed = math.sqrt(vx*vx + vz*vz)
end

function OnDestroy()
    if NavAgent then
        NavAgent.stop()
    end
    Debug.log("EnemyWander: Destroyed")
end
