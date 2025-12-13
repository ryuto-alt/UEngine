-- RotatingCube.lua
-- オブジェクトを回転させるサンプルスクリプト

-- public変数（Inspectorに表示される）
rotationSpeed = 45.0  -- 度/秒
axis = "y"            -- 回転軸 (x, y, z)

-- ローカル変数
local totalRotation = 0

function Awake()
    print("RotatingCube script initialized")
end

function Start()
    Debug.log("RotatingCube attached to: " .. gameObject.name)
end

function Update(deltaTime)
    -- 回転量を計算
    local rotation = rotationSpeed * deltaTime
    totalRotation = totalRotation + rotation
    
    -- 現在の回転を取得
    local rx, ry, rz = transform.getRotation()
    
    -- 指定軸で回転
    if axis == "x" then
        transform.setRotation(rx + rotation, ry, rz)
    elseif axis == "y" then
        transform.setRotation(rx, ry + rotation, rz)
    elseif axis == "z" then
        transform.setRotation(rx, ry, rz + rotation)
    end
end

function OnDestroy()
    Debug.log(string.format("RotatingCube destroyed. Total rotation: %.2f degrees", totalRotation))
end
