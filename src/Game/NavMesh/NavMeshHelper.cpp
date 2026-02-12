#include "NavMeshHelper.h"
#include "NavMesh.h"
#include "NavMeshBuilder.h"
#include "DetourNavMeshQuery.h"
#include <cmath>
#include <algorithm>

Vector3 NavMeshHelper::ClampToNavMesh(
	const Vector3& position,
	NavMesh* navMesh,
	const Vector3& extents
) {
	if (!navMesh || !navMesh->IsValid()) {
		return position;
	}

	// Use NavMesh's cached query instead of allocating a new one every frame
	return navMesh->FindNearestPoint(position, extents);
}

void NavMeshHelper::FollowPath(
	Vector3& position,
	float& currentRotationY,
	float& currentSpeed,
	std::vector<Vector3>& currentPath,
	int& currentWaypointIndex,
	float moveSpeed,
	float deltaTime,
	NavMesh* navMesh,
	bool* isAtCorner,
	float* cornerSlowdownFactor,
	bool aggressiveCorner
) {
	if (currentPath.empty() || currentWaypointIndex >= static_cast<int>(currentPath.size())) {
		return;
	}

	// Advance past any waypoints already within reach (don't skip a frame per waypoint)
	const float WAYPOINT_REACH_THRESHOLD = 0.5f;
	while (currentWaypointIndex < static_cast<int>(currentPath.size())) {
		const Vector3& wp = currentPath[currentWaypointIndex];
		float dx = wp.x - position.x;
		float dz = wp.z - position.z;
		if (std::sqrt(dx * dx + dz * dz) >= WAYPOINT_REACH_THRESHOLD) break;
		currentWaypointIndex++;
	}

	if (currentWaypointIndex >= static_cast<int>(currentPath.size())) {
		currentPath.clear();
		currentWaypointIndex = 0;
		return;
	}

	const Vector3& targetWaypoint = currentPath[currentWaypointIndex];

	// 目標ウェイポイントへの方向
	float toWpX = targetWaypoint.x - position.x;
	float toWpZ = targetWaypoint.z - position.z;
	float distToWp = std::sqrt(toWpX * toWpX + toWpZ * toWpZ);
	if (distToWp > 0.001f) {
		toWpX /= distToWp;
		toWpZ /= distToWp;
	}

	// 目標回転角
	float targetRotationY = std::atan2(toWpX, toWpZ);

	// 現在の向きと目標方向の角度差を計算
	float angleDiff = targetRotationY - currentRotationY;
	while (angleDiff > 3.14159f) angleDiff -= 2.0f * 3.14159f;
	while (angleDiff < -3.14159f) angleDiff += 2.0f * 3.14159f;
	float absAngleDiff = std::abs(angleDiff);

	// 角度差に基づいて減速と回転制限（パス再計算に依存しない）
	bool atCorner = false;
	float slowdownFactor = 1.0f;
	float rotationSpeed = 4.0f; // 通常時も穏やかな回転

	const float TURN_THRESHOLD = 0.25f; // 約14度以上で減速開始
	if (absAngleDiff > TURN_THRESHOLD) {
		atCorner = true;
		if (aggressiveCorner) {
			// 追跡時: 大幅に減速してアウトコースで膨らむ
			float t = std::min((absAngleDiff - TURN_THRESHOLD) / (1.5708f - TURN_THRESHOLD), 1.0f);
			slowdownFactor = 0.70f - t * 0.50f; // 0.70 〜 0.20
			float tRot = std::min(absAngleDiff / 3.14159f, 1.0f);
			rotationSpeed = 0.6f + (1.0f - tRot) * 1.8f; // 0.6〜2.4 rad/s
		} else {
			// 徘徊・捜索時: 普通のコーナリング
			slowdownFactor = 0.85f;
			rotationSpeed = 3.5f;
		}
	}

	// 回転適用
	float rotationStep = rotationSpeed * deltaTime;
	if (absAngleDiff > rotationStep) {
		currentRotationY += (angleDiff > 0 ? rotationStep : -rotationStep);
	} else {
		currentRotationY = targetRotationY;
	}

	// 移動方向は現在の向き（回転が遅い分、外側に大きく膨らむ）
	float moveDirX = std::sin(currentRotationY);
	float moveDirZ = std::cos(currentRotationY);

	// 速度制御
	float targetSpeed = moveSpeed * slowdownFactor;
	if (atCorner) {
		// 曲がっている最中は即座に減速
		currentSpeed = std::min(currentSpeed, targetSpeed);
		float lerpRate = 1.0f - std::exp(-15.0f * deltaTime);
		currentSpeed += (targetSpeed - currentSpeed) * lerpRate;
	} else {
		// 直線では通常の加速
		float lerpRate = 1.0f - std::exp(-5.0f * deltaTime);
		currentSpeed += (targetSpeed - currentSpeed) * lerpRate;
	}

	// 移動
	Vector3 newPosition = position;
	newPosition.x += moveDirX * currentSpeed * deltaTime;
	newPosition.z += moveDirZ * currentSpeed * deltaTime;

	// NavMesh上の有効な位置に補正
	newPosition = ClampToNavMesh(newPosition, navMesh);

	position = newPosition;

	// 出力パラメータ
	if (isAtCorner) *isAtCorner = atCorner;
	if (cornerSlowdownFactor) *cornerSlowdownFactor = slowdownFactor;
}
