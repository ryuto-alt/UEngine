-- EnemyScripts.lua
-- WASDキーでEnemyをアニメーション付きで移動させるスクリプト

-- public変数（Inspectorに表示される）
moveSpeed = 12.0
rotateSpeed = 500.0

-- ローカル変数
local isMoving = false
local currentYaw = 0

-- math.atan2のフォールバック
local atan2 = math.atan2 or function(y, x)
    if x > 0 then
        return math.atan(y / x)
    elseif x < 0 then
        if y >= 0 then
            return math.atan(y / x) + math.pi
        else
            return math.atan(y / x) - math.pi
        end
    else
        if y > 0 then return math.pi / 2
        elseif y < 0 then return -math.pi / 2
        else return 0 end
    end
end

-- math.degのフォールバック
local deg = math.deg or function(rad)
    return rad * 57.2957795
end

function Awake()
    Debug.log("EnemyScripts initialized on: " .. gameObject.name)
end

function Start()
    -- 初期位置を取得
    local x, y, z = transform.getPosition()
    Debug.log(string.format("Enemy position: (%.2f, %.2f, %.2f)", x, y, z))
    
    -- 初期回転を取得
    local rx, ry, rz = transform.getRotation()
    currentYaw = ry
end

function Update(deltaTime)
    -- 入力がない場合のデフォルト
    local moveX = 0
    local moveZ = 0
    local wasMoving = isMoving
    isMoving = false
    
    -- WASD入力を取得
    if Input then
        local horizontal = Input.getAxis("Horizontal")
        local vertical = Input.getAxis("Vertical")
        
        moveX = horizontal
        moveZ = vertical
        
        -- 移動中かどうかを判定
        if math.abs(horizontal) > 0.1 or math.abs(vertical) > 0.1 then
            isMoving = true
        end
    end
    
    -- 移動処理
    if isMoving then
        -- 移動方向に回転
        if math.abs(moveX) > 0.1 or math.abs(moveZ) > 0.1 then
            local targetYaw = deg(atan2(moveX, moveZ))
            
            -- 現在の回転から目標の回転へ補間
            local diff = targetYaw - currentYaw
            
            -- 角度の差を-180〜180に正規化
            while diff > 180 do diff = diff - 360 end
            while diff < -180 do diff = diff + 360 end
            
            -- 回転を適用（補間）
            local maxRotate = rotateSpeed * deltaTime
            if math.abs(diff) < maxRotate then
                currentYaw = targetYaw
            else
                if diff > 0 then
                    currentYaw = currentYaw + maxRotate
                else
                    currentYaw = currentYaw - maxRotate
                end
            end
            
            -- 回転を設定
            transform.setRotation(0, currentYaw, 0)
        end
        
        -- 移動を適用
        local speed = moveSpeed * deltaTime
        transform.translate(moveX * speed, 0, moveZ * speed)
        
        -- 移動アニメーションを再生
        if Animator and not wasMoving then
            Animator.play("Walk", true)
            Debug.log("Playing Walk animation")
        end
    else
        -- アイドルアニメーションを再生
        if Animator and wasMoving then
            Animator.play("Idle", true)
            Debug.log("Playing Idle animation")
        end
    end
end

function OnDestroy()
    Debug.log("EnemyScripts destroyed")
end
