-- EnemyPatrol.lua
-- NavMeshを使って指定ポイント間をパトロールするEnemyスクリプト

-- 設定をJSONから読み込み
local config = Config and Config.loadJson("assets/config/enemy_patrol.json") or {}

-- public変数（JSONのデフォルト値を使用、Inspectorで上書き可能）
moveSpeed = config.moveSpeed or 4.0
waitTime = config.waitTime or 2.0
loopPatrol = (config.loopPatrol ~= nil) and config.loopPatrol or true

-- 定数（JSONから読み込み）
local STOPPING_DISTANCE = config.stoppingDistance or 0.5

-- パトロールポイント（JSONから読み込み）
local patrolPoints = config.patrolPoints or {
    {x = 0, y = 0, z = 0},
    {x = 10, y = 0, z = 0},
    {x = 10, y = 0, z = 10},
    {x = 0, y = 0, z = 10}
}

-- ローカル変数
local initialized = false
local lastState = ""

function Awake()
    Debug.log("EnemyPatrol: Awake - " .. gameObject.name)
end

function Start()
    Debug.log("EnemyPatrol: Start")

    if not NavAgent then
        Debug.error("EnemyPatrol: NavAgent component not found!")
        return
    end

    -- NavAgentのパラメータ設定
    NavAgent.setSpeed(moveSpeed)
    NavAgent.setWaitTime(waitTime)
    NavAgent.setStoppingDistance(STOPPING_DISTANCE)

    -- パトロールポイントを追加
    NavAgent.clearPatrolPoints()
    for i, point in ipairs(patrolPoints) do
        NavAgent.addPatrolPoint(point.x, point.y, point.z)
        Debug.log(string.format("EnemyPatrol: Added point %d (%.1f, %.1f, %.1f)",
            i, point.x, point.y, point.z))
    end

    -- パトロール開始
    NavAgent.startPatrol(loopPatrol)

    initialized = true
    Debug.log("EnemyPatrol: Patrol started")
end

function Update(deltaTime)
    if not initialized or not NavAgent then
        return
    end

    local state = NavAgent.getState()
    if state ~= lastState then
        Debug.log("EnemyPatrol: State changed to " .. state)
        lastState = state

        if Animator then
            if state == "patrolling" or state == "moving" then
                Animator.play("Walk", true)
            else
                Animator.play("Idle", true)
            end
        end
    end
end

function OnDestroy()
    if NavAgent then
        NavAgent.stop()
    end
    Debug.log("EnemyPatrol: Destroyed")
end

-- パトロールポイントを動的に設定する関数（外部から呼び出し可能）
function SetPatrolPoints(points)
    patrolPoints = points
    if initialized and NavAgent then
        NavAgent.clearPatrolPoints()
        for _, point in ipairs(points) do
            NavAgent.addPatrolPoint(point.x, point.y, point.z)
        end
        NavAgent.startPatrol(loopPatrol)
    end
end
