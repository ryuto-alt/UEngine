-- EnemyScripts.lua
-- NavMesh上を巡回する敵AI（Dark Deception風）

-- public変数（Inspectorに表示される）
patrolSpeed = 3.0           -- 巡回時の移動速度
patrolWaitTime = 1.5        -- 巡回ポイントでの待機時間
patrolRadius = 15.0         -- 巡回半径
minPatrolDistance = 3.0     -- 最低巡回距離（近すぎる目標を避ける）

-- ローカル変数
local state = "init"        -- init, patrol, wait
local waitTimer = 0
local patrolCenter = nil
local currentTarget = nil
local initialized = false
local frameCount = 0
local pathSetTime = 0       -- パス設定時刻（再設定防止用）
local MIN_PATH_INTERVAL = 0.5  -- 最低パス再設定間隔（秒）

function Awake()
    Debug.log("[Enemy] Awake: " .. gameObject.name)
end

function Start()
    Debug.log("[Enemy] Start: " .. gameObject.name)
    
    -- NavMeshAgent APIが存在するか確認
    if not NavMeshAgent then
        Debug.error("[Enemy] NavMeshAgent API not found!")
        return
    end
    
    -- コンポーネントが存在するか確認
    if not NavMeshAgent.exists() then
        Debug.error("[Enemy] NavMeshAgentComponent not found!")
        Debug.error("[Enemy] Please add NavMeshAgentComponent to this GameObject")
        return
    end
    
    Debug.log("[Enemy] NavMeshAgentComponent found!")
    initialized = true
    
    -- 現在位置を取得して巡回の中心に設定
    local x, y, z = transform.getPosition()
    patrolCenter = {x = x, y = y, z = z}
    Debug.log(string.format("[Enemy] Patrol center: (%.2f, %.2f, %.2f)", x, y, z))
    
    -- NavMeshに吸着
    NavMeshAgent.snapToNavMesh()
    
    -- 速度設定
    NavMeshAgent.setSpeed(patrolSpeed)
    NavMeshAgent.setAngularSpeed(360)  -- 素早く回転
    NavMeshAgent.setAcceleration(10)   -- 加速を速く
    NavMeshAgent.setStoppingDistance(0.5)  -- 停止距離を少し大きく
    
    -- 最初の巡回目標を設定
    state = "patrol"
    SetNewPatrolTarget()
end

function Update(deltaTime)
    if not initialized then return end
    
    frameCount = frameCount + 1
    pathSetTime = pathSetTime + deltaTime
    
    -- 状態に応じた処理
    if state == "patrol" then
        UpdatePatrol(deltaTime)
    elseif state == "wait" then
        UpdateWait(deltaTime)
    end
    
    -- アニメーション更新
    UpdateAnimation()
end

function UpdatePatrol(deltaTime)
    local hasPath = NavMeshAgent.hasPath()
    local isMoving = NavMeshAgent.isMoving()
    local reached = NavMeshAgent.hasReachedDestination()
    
    -- デバッグ出力（最初の数フレームと10フレームごと）
    if frameCount <= 5 or frameCount % 60 == 0 then
        local speed = NavMeshAgent.getCurrentSpeed()
        Debug.log(string.format("[Enemy] frame=%d hasPath=%s moving=%s reached=%s speed=%.2f pathAge=%.2f",
            frameCount, tostring(hasPath), tostring(isMoving), tostring(reached), speed, pathSetTime))
    end
    
    -- 目的地に到達した場合
    if reached then
        Debug.log("[Enemy] Reached destination, waiting...")
        state = "wait"
        waitTimer = patrolWaitTime
        NavMeshAgent.stop()
        return
    end
    
    -- パスが有効で移動中なら何もしない
    if hasPath and isMoving then
        return
    end
    
    -- パスがない場合、一定時間経過後に新しい目標を設定
    if not hasPath and pathSetTime >= MIN_PATH_INTERVAL then
        Debug.log("[Enemy] No path after interval, finding new target...")
        SetNewPatrolTarget()
    end
end

function UpdateWait(deltaTime)
    waitTimer = waitTimer - deltaTime
    if waitTimer <= 0 then
        state = "patrol"
        SetNewPatrolTarget()
    end
end

function SetNewPatrolTarget()
    if not patrolCenter then return end
    
    -- パス再設定タイマーをリセット
    pathSetTime = 0
    
    -- 現在位置を取得
    local currentX, currentY, currentZ = transform.getPosition()
    
    -- 複数回試行して、適切な距離の目標を探す
    local maxAttempts = 5
    local success = false
    local targetX, targetY, targetZ
    
    for attempt = 1, maxAttempts do
        -- NavMesh上のランダムな点を取得（範囲内）
        success, targetX, targetY, targetZ = NavMeshAgent.findRandomPointInRadius(
            patrolCenter.x, patrolCenter.y, patrolCenter.z, patrolRadius)
        
        if success then
            -- 現在位置との距離をチェック
            local dx = targetX - currentX
            local dz = targetZ - currentZ
            local dist = math.sqrt(dx * dx + dz * dz)
            
            if dist >= minPatrolDistance then
                break  -- 十分な距離がある
            else
                success = false  -- 近すぎるので再試行
            end
        end
    end
    
    -- 範囲内で見つからなければ全体から検索
    if not success then
        success, targetX, targetY, targetZ = NavMeshAgent.findRandomPoint()
    end
    
    if success then
        currentTarget = {x = targetX, y = targetY, z = targetZ}
        Debug.log(string.format("[Enemy] New target: (%.2f, %.2f, %.2f)", targetX, targetY, targetZ))
        
        local pathSuccess = NavMeshAgent.setDestination(targetX, targetY, targetZ)
        if not pathSuccess then
            Debug.warn("[Enemy] Failed to find path to target")
            state = "wait"
            waitTimer = 0.5
        end
    else
        Debug.warn("[Enemy] Failed to find random point on NavMesh")
        state = "wait"
        waitTimer = 1.0
    end
end

function UpdateAnimation()
    if not Animator then return end
    
    local speed = NavMeshAgent.getCurrentSpeed()
    if speed > 0.1 then
        Animator.play("Walk", true)
    else
        Animator.play("Idle", true)
    end
end

function OnDestroy()
    Debug.log("[Enemy] Destroyed: " .. gameObject.name)
end
