-- EnemyPatrol.lua
-- NavMeshを使って指定ポイント間をパトロールするEnemyスクリプト

-- public変数（Inspectorに表示される）
moveSpeed = 4.0          -- 移動速度
waitTime = 2.0           -- 各ポイントでの待機時間（秒）
loopPatrol = true        -- ループするか（falseなら往復）

-- パトロールポイント（Inspectorから設定できないので、ここで定義）
-- 実際のゲームではシーンに配置したマーカーから取得する
local patrolPoints = {
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
    NavAgent.setStoppingDistance(0.5)
    
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
