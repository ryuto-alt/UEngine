-- EnemyWander.lua
-- NavMeshを使って自動徘徊するEnemyスクリプト
-- Dark Deception風の狭い迷路でも動作

-- 設定をJSONから読み込み
local config = Config and Config.loadJson("assets/config/enemy_wander.json") or {}

-- public変数（JSONのデフォルト値を使用、Inspectorで上書き可能）
wanderRadius = config.wanderRadius or 15.0
moveSpeed = config.moveSpeed or 7.5
waitTime = config.waitTime or 0.0
angularSpeed = config.angularSpeed or 720.0
initialYaw = config.initialYaw or 0.0

-- 定数（JSONから読み込み）
local STOPPING_DISTANCE = config.stoppingDistance or 0.5

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
    NavAgent.setStoppingDistance(STOPPING_DISTANCE)

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
end

function OnDestroy()
    if NavAgent then
        NavAgent.stop()
    end
    Debug.log("EnemyWander: Destroyed")
end
