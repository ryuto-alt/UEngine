#include "pch.h"
#include "MeshCollisionSystem.h"
#include "../Core/Scene.h"
#include "../Core/GameObject.h"
#include "../Core/Transform.h"
#include "../Physics/CapsuleColliderComponent.h"
#include "../Physics/MeshColliderComponent.h"
#include "../Physics/RigidbodyComponent.h"
#include "../Math/Matrix.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <mutex>

#ifdef WITH_EDITOR
#include "../../Game/UI/EditorUI.h"
#endif

// ── デバッグログ ──
namespace {

static constexpr bool kEnableStepLog = true;
static constexpr const char* kLogPath = "C:/Users/ryuto/Documents/github/step_debug.log";
static std::mutex sLogMutex;
static int sFrameCount = 0;
static int sLoggedFrames = 0;
// 毎フレーム出すと巨大になるので、ステップアップ発生時 or 30フレームごとに出力
static constexpr int kLogInterval = 30;

void StepLog(const char* fmt, ...) {
    if (!kEnableStepLog) return;
    std::lock_guard<std::mutex> lock(sLogMutex);
    static FILE* fp = nullptr;
    if (!fp) {
        fopen_s(&fp, kLogPath, "w");
        if (!fp) return;
        fprintf(fp, "=== MeshCollisionSystem Step Debug Log ===\n\n");
    }
    va_list args;
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);
    fflush(fp);
}

} // anonymous namespace

namespace UnoEngine {

void MeshCollisionSystem::OnSceneStart(Scene* /*scene*/) {}
void MeshCollisionSystem::OnSceneEnd(Scene* /*scene*/) {
    capsuleEntities_.clear();
    meshEntities_.clear();
}

void MeshCollisionSystem::OnUpdate(Scene* scene, float deltaTime) {
    if (!IsEnabled() || !scene) return;

#ifdef WITH_EDITOR
    auto* editorUI = scene->GetEditorUI();
    if (editorUI && !editorUI->IsPlaying()) return;
#endif

    GatherComponents(scene);
    sFrameCount++;

    for (auto& capsuleEnt : capsuleEntities_) {
        ProcessCapsule(capsuleEnt);
    }
}

void MeshCollisionSystem::GatherComponents(Scene* scene) {
    capsuleEntities_.clear();
    meshEntities_.clear();

    for (auto& obj : scene->GetGameObjects()) {
        if (!obj || !obj->IsActive()) continue;

        auto* capsule = obj->GetComponent<CapsuleColliderComponent>();
        if (capsule && capsule->IsEnabled()) {
            capsuleEntities_.push_back({obj.get(), capsule});
        }

        auto* meshCollider = obj->GetComponent<MeshColliderComponent>();
        if (meshCollider && meshCollider->IsEnabled() && meshCollider->IsBuilt()) {
            meshEntities_.push_back({obj.get(), meshCollider});
        }
    }
}

bool MeshCollisionSystem::QueryContacts(const Capsule& worldCapsule, const MeshEntity& meshEnt,
                                         std::vector<MeshContact>& outContacts) const {
    const auto* bvh = meshEnt.meshCollider->GetBVH();

    Matrix4x4 meshWorldMatrix = meshEnt.object->GetTransform().GetWorldMatrix();
    Matrix4x4 meshInverse = meshWorldMatrix.Inverse();

    // Transform capsule into mesh local space
    Capsule localCapsule;
    localCapsule.base = meshInverse.TransformPoint(worldCapsule.base);
    localCapsule.tip  = meshInverse.TransformPoint(worldCapsule.tip);

    // Conservative radius scaling
    float invScaleX = meshInverse.TransformDirection(Vector3::UnitX()).Length();
    float invScaleY = meshInverse.TransformDirection(Vector3::UnitY()).Length();
    float invScaleZ = meshInverse.TransformDirection(Vector3::UnitZ()).Length();
    float maxInvScale = std::max({invScaleX, invScaleY, invScaleZ});
    localCapsule.radius = worldCapsule.radius * maxInvScale;

    BoundingBox localCapsuleAABB = localCapsule.GetBoundingAABB();

    std::vector<uint32_t> candidateTriangles;
    bvh->QueryAABB(localCapsuleAABB, candidateTriangles);
    if (candidateTriangles.empty()) return false;

    bool anyHit = false;
    for (uint32_t triIdx : candidateTriangles) {
        const Triangle& tri = bvh->GetTriangle(triIdx);
        PenetrationResult result = CapsuleTriangleIntersect(localCapsule, tri);

        if (result.hit) {
            MeshContact contact;
            // Transform normal to world space
            contact.normal = meshWorldMatrix.TransformDirection(result.normal).Normalize();
            // Triangle face normal (for ground classification)
            Vector3 localFaceNormal = tri.GetNormal();
            contact.faceNormal = meshWorldMatrix.TransformDirection(localFaceNormal).Normalize();
            // Scale depth along the contact normal direction
            Vector3 localNormal = result.normal;
            Vector3 worldScaledNormal = meshWorldMatrix.TransformDirection(localNormal);
            contact.depth = result.depth * worldScaledNormal.Length();
            outContacts.push_back(contact);
            anyHit = true;
        }
    }
    return anyHit;
}

void MeshCollisionSystem::ProcessCapsule(CapsuleEntity& capsuleEnt) {
    auto& transform = capsuleEnt.object->GetTransform();
    auto* rb = capsuleEnt.object->GetComponent<RigidbodyComponent>();
    const float maxStepHeight = capsuleEnt.capsule->GetMaxStepHeight();
    bool grounded = false;
    bool hasWallContact = false;

    // 最終パスの接触法線を保持（速度スライド用）
    std::vector<Vector3> slideNormals;

    // デペネトレーション前の位置を保存（ステップアップの基準点）
    Vector3 posBeforeDepenetration = transform.GetLocalPosition();

    // ログ出力判定（定期 or ステップアップ発生時）
    bool shouldLog = kEnableStepLog && (sFrameCount % kLogInterval == 0);
    bool stepUpTriggered = false;

    // ── 通常デペネトレーション ──
    // faceNormal（三角形表面法線）で地面/壁を判定
    // 垂直pushはmaxStepHeightで制限（壁の上面に飛び乗るのを防止）
    int totalContactCount = 0;
    int groundContactCount = 0;
    int wallContactCount = 0;
    float totalVerticalPush = 0.0f;

    for (uint32_t pass = 0; pass < kMaxDepenetrationPasses; ++pass) {
        Capsule worldCapsule = capsuleEnt.capsule->GetWorldCapsule();

        std::vector<MeshContact> contacts;
        for (auto& meshEnt : meshEntities_) {
            if (capsuleEnt.object == meshEnt.object) continue;
            QueryContacts(worldCapsule, meshEnt, contacts);
        }

        if (contacts.empty()) break;

        float bestGroundDepth = 0.0f;
        Vector3 bestGroundNormal = Vector3::Zero();
        float bestWallDepth = 0.0f;
        Vector3 bestWallNormal = Vector3::Zero();

        slideNormals.clear();

        for (const auto& contact : contacts) {
            if (contact.depth <= 0.0f) continue;

            slideNormals.push_back(contact.normal);
            totalContactCount++;

            // 通常デペネではcontact.normalで判定（横からの接触は壁扱い）
            // faceNormalはステップアップでのみ使用
            if (contact.normal.GetY() > kGroundNormalThreshold) {
                if (contact.depth > bestGroundDepth) {
                    bestGroundDepth = contact.depth;
                    bestGroundNormal = contact.normal;
                }
                grounded = true;
                groundContactCount++;
            } else {
                if (contact.depth > bestWallDepth) {
                    bestWallDepth = contact.depth;
                    bestWallNormal = contact.normal;
                }
                hasWallContact = true;
                wallContactCount++;
            }
        }

        Vector3 totalPush = Vector3::Zero();
        if (bestGroundDepth > 0.0f) {
            totalPush = totalPush + bestGroundNormal * (bestGroundDepth + kSkinWidth);
        }
        if (bestWallDepth > 0.0f) {
            totalPush = totalPush + bestWallNormal * (bestWallDepth + kSkinWidth);
        }

        if (totalPush.LengthSq() < 1e-8f) break;

        // 垂直pushをmaxStepHeightで制限（壁の上に飛び乗るのを防止）
        float pushY = totalPush.GetY();
        if (pushY > 0.0f) {
            float remaining = maxStepHeight - totalVerticalPush;
            if (remaining <= 0.0f) {
                totalPush = Vector3(totalPush.GetX(), 0.0f, totalPush.GetZ());
            } else if (pushY > remaining) {
                totalPush = Vector3(totalPush.GetX(), remaining, totalPush.GetZ());
            }
            totalVerticalPush += totalPush.GetY();
        }

        if (totalPush.LengthSq() < 1e-8f) break;

        Vector3 pos = transform.GetLocalPosition();
        transform.SetLocalPosition(pos + totalPush);
    }

    Vector3 posAfterNormal = transform.GetLocalPosition();

    // ── ステップアップ（段差乗り越え）──
    // posBeforeDepenetration（段差に埋まった位置）から持ち上げて段の上面を探す。
    // faceNormalで地面判定 → 垂直成分のみでpushして段の上に着地。
    // 壁pushを適用しないので、次の段の壁で押し戻されない。
    bool stepUpSuccess = false;
    int stepUpCheckedHeight = -1;
    int stepUpGroundFoundAt = -1;
    float stepUpFinalY = 0.0f;

    if (grounded && hasWallContact && maxStepHeight > 0.0f) {
        stepUpTriggered = true;
        constexpr int kStepChecks = 16;
        float stepInc = maxStepHeight / static_cast<float>(kStepChecks);

        // 上から下へ検索 — 最も高い着地面を見つける（床ではなくステップ面）
        for (int i = kStepChecks; i >= 1; --i) {
            float h = stepInc * static_cast<float>(i);
            transform.SetLocalPosition(posBeforeDepenetration + Vector3(0.0f, h, 0.0f));
            stepUpCheckedHeight = i;

            // この高さで地面(faceNormal)があるか確認
            Capsule testCapsule = capsuleEnt.capsule->GetWorldCapsule();
            std::vector<MeshContact> testContacts;
            for (auto& meshEnt : meshEntities_) {
                if (capsuleEnt.object == meshEnt.object) continue;
                QueryContacts(testCapsule, meshEnt, testContacts);
            }

            bool foundGround = false;
            for (const auto& c : testContacts) {
                if (c.depth > 0.0f && c.faceNormal.GetY() > kGroundNormalThreshold) {
                    foundGround = true;
                    break;
                }
            }

            if (!foundGround) continue;

            stepUpGroundFoundAt = i;

            // 着地面あり → 垂直成分のみでデペネトレーション
            for (uint32_t pass = 0; pass < kMaxDepenetrationPasses; ++pass) {
                Capsule wc = capsuleEnt.capsule->GetWorldCapsule();
                std::vector<MeshContact> contacts;
                for (auto& meshEnt : meshEntities_) {
                    if (capsuleEnt.object == meshEnt.object) continue;
                    QueryContacts(wc, meshEnt, contacts);
                }
                if (contacts.empty()) break;

                // 地面三角形からの接触のみ、垂直成分だけ適用
                float bestUpPush = 0.0f;
                for (const auto& c : contacts) {
                    if (c.depth <= 0.0f) continue;
                    if (c.faceNormal.GetY() > kGroundNormalThreshold) {
                        float upPush = c.faceNormal.GetY() * c.depth;
                        if (upPush > bestUpPush) {
                            bestUpPush = upPush;
                        }
                    }
                }
                if (bestUpPush < 1e-8f) break;
                Vector3 p = transform.GetLocalPosition();
                transform.SetLocalPosition(p + Vector3(0.0f, bestUpPush, 0.0f));
            }

            // ステップアップ後の位置を検証
            // 1) 通常結果より高いか  2) maxStepHeightを超えていないか
            Vector3 stepPos = transform.GetLocalPosition();
            stepUpFinalY = stepPos.GetY();
            float heightGain = stepPos.GetY() - posBeforeDepenetration.GetY();
            if (stepPos.GetY() > posAfterNormal.GetY() + kSkinWidth
                && heightGain <= maxStepHeight + kSkinWidth) {
                stepUpSuccess = true;
                grounded = true;
                break;
            }
        }

        if (!stepUpSuccess) {
            transform.SetLocalPosition(posAfterNormal);
        }
    }

    // ── デバッグログ出力 ──
    if (shouldLog || stepUpTriggered) {
        Vector3 finalPos = transform.GetLocalPosition();
        float heightChange = finalPos.GetY() - posBeforeDepenetration.GetY();

        StepLog("[Frame %d] obj=%s  maxStep=%.3f\n",
            sFrameCount,
            capsuleEnt.object->GetName().c_str(),
            maxStepHeight);
        StepLog("  posBefore:  (%.3f, %.3f, %.3f)\n",
            posBeforeDepenetration.GetX(), posBeforeDepenetration.GetY(), posBeforeDepenetration.GetZ());
        StepLog("  posAfterNormal: (%.3f, %.3f, %.3f)\n",
            posAfterNormal.GetX(), posAfterNormal.GetY(), posAfterNormal.GetZ());
        StepLog("  posFinal:   (%.3f, %.3f, %.3f)  heightChange=%.4f\n",
            finalPos.GetX(), finalPos.GetY(), finalPos.GetZ(), heightChange);
        StepLog("  contacts: total=%d  ground=%d  wall=%d\n",
            totalContactCount, groundContactCount, wallContactCount);
        StepLog("  grounded=%s  wallContact=%s\n",
            grounded ? "YES" : "no", hasWallContact ? "YES" : "no");

        if (stepUpTriggered) {
            StepLog("  >>> STEP-UP triggered! maxStepH=%.3f\n", maxStepHeight);
            StepLog("      checked %d heights, ground found at height #%d\n",
                stepUpCheckedHeight, stepUpGroundFoundAt);
            StepLog("      result: %s  finalY=%.4f  normalY=%.4f  diff=%.4f\n",
                stepUpSuccess ? "SUCCESS" : "FAILED",
                stepUpFinalY,
                posAfterNormal.GetY(),
                stepUpFinalY - posAfterNormal.GetY());
        }
        StepLog("\n");
        sLoggedFrames++;
    }

    if (rb) {
        rb->SetGrounded(grounded);

        // 壁スライド（接触法線に沿って速度を滑らせる）
        Vector3 velocity = rb->GetVelocity();
        for (const auto& normal : slideNormals) {
            float vn = velocity.Dot(normal);
            if (vn < 0.0f) {
                velocity = velocity - normal * vn;
            }
        }
        rb->SetVelocity(velocity);

        // 接地時: X/Z速度をゼロに（移動はtransform.translateで行うため不要）
        // Y速度は正（ジャンプ中）なら維持、負（落下中）ならゼロ
        if (grounded) {
            Vector3 vel = rb->GetVelocity();
            float newY = vel.GetY() < 0.0f ? 0.0f : vel.GetY();
            rb->SetVelocity(Vector3(0.0f, newY, 0.0f));
        }
    }
}

} // namespace UnoEngine
