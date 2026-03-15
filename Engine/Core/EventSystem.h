#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <any>

#include "NonCopyable.h"

namespace UnoEngine {

// イベント基底構造体
struct Event {
    virtual ~Event() = default;
};

// コールバック識別子
using EventCallbackId = uint64_t;

// 型消去されたコールバックホルダー
class IEventHandler {
public:
    virtual ~IEventHandler() = default;
    virtual void Invoke(const Event& event) = 0;
    EventCallbackId GetId() const { return id_; }

protected:
    EventCallbackId id_ = 0;
};

// 型付きコールバックハンドラー
template<typename TEvent>
class EventHandler : public IEventHandler {
public:
    using Callback = std::function<void(const TEvent&)>;

    EventHandler(EventCallbackId id, Callback callback)
        : callback_(std::move(callback)) {
        id_ = id;
    }

    void Invoke(const Event& event) override {
        callback_(static_cast<const TEvent&>(event));
    }

private:
    Callback callback_;
};

// イベントシステム - シングルトン
class EventSystem : public NonCopyable {
public:
    ~EventSystem() = default;

    // シングルトンアクセス
    static EventSystem& GetInstance();

    // イベント購読 - コールバックIDを返す
    template<typename TEvent>
    EventCallbackId Subscribe(std::function<void(const TEvent&)> callback);

    // 購読解除
    void Unsubscribe(EventCallbackId id);

    // 即時発火 - 引数からイベントを構築して配信
    template<typename TEvent, typename... Args>
    void Fire(Args&&... args);

    // 即時配信 - 構築済みイベントを配信
    template<typename TEvent>
    void Dispatch(const TEvent& event);

    // 遅延キュー - 引数からイベントを構築してキューに追加
    template<typename TEvent, typename... Args>
    void Queue(Args&&... args);

    // キュー内の全イベントを処理
    void ProcessQueue();

    // 全購読とキューをクリア（シーン切り替え時など）
    void ClearAll();

    // 特定イベント型の購読をクリア
    template<typename TEvent>
    void ClearEvent();

    // デバッグ用：購読数を取得
    size_t GetSubscriberCount() const;

private:
    EventSystem() = default;

    EventCallbackId GenerateId();

    // イベント型ごとのハンドラーリスト
    std::unordered_map<std::type_index, std::vector<std::unique_ptr<IEventHandler>>> handlers_;

    // コールバックIDからイベント型へのマッピング（Unsubscribe用）
    std::unordered_map<EventCallbackId, std::type_index> idToType_;

    // 遅延イベントキュー
    struct QueuedEvent {
        std::type_index type;
        std::shared_ptr<Event> event;
    };
    std::vector<QueuedEvent> eventQueue_;

    // ID生成カウンター
    EventCallbackId nextId_ = 1;
};

// RAII購読ガード - スコープ終了時に自動で購読解除
class EventSubscription {
public:
    EventSubscription() = default;

    explicit EventSubscription(EventCallbackId id)
        : id_(id), active_(true) {}

    ~EventSubscription() {
        Unsubscribe();
    }

    // ムーブのみ許可
    EventSubscription(const EventSubscription&) = delete;
    EventSubscription& operator=(const EventSubscription&) = delete;

    EventSubscription(EventSubscription&& other) noexcept
        : id_(other.id_), active_(other.active_) {
        other.active_ = false;
        other.id_ = 0;
    }

    EventSubscription& operator=(EventSubscription&& other) noexcept {
        if (this != &other) {
            Unsubscribe();
            id_ = other.id_;
            active_ = other.active_;
            other.active_ = false;
            other.id_ = 0;
        }
        return *this;
    }

    // 手動購読解除
    void Unsubscribe() {
        if (active_) {
            EventSystem::GetInstance().Unsubscribe(id_);
            active_ = false;
            id_ = 0;
        }
    }

    // 購読を解放（自動解除を無効化）
    EventCallbackId Release() {
        active_ = false;
        EventCallbackId id = id_;
        id_ = 0;
        return id;
    }

    bool IsActive() const { return active_; }
    EventCallbackId GetId() const { return id_; }

private:
    EventCallbackId id_ = 0;
    bool active_ = false;
};

// ============================================================
// テンプレート実装
// ============================================================

template<typename TEvent>
EventCallbackId EventSystem::Subscribe(std::function<void(const TEvent&)> callback) {
    static_assert(std::is_base_of_v<Event, TEvent>, "TEvent must derive from Event");

    auto id = GenerateId();
    auto handler = std::make_unique<EventHandler<TEvent>>(id, std::move(callback));

    auto typeIndex = std::type_index(typeid(TEvent));
    handlers_[typeIndex].push_back(std::move(handler));
    idToType_.emplace(id, typeIndex);

    return id;
}

template<typename TEvent, typename... Args>
void EventSystem::Fire(Args&&... args) {
    static_assert(std::is_base_of_v<Event, TEvent>, "TEvent must derive from Event");

    TEvent event{std::forward<Args>(args)...};
    Dispatch(event);
}

template<typename TEvent>
void EventSystem::Dispatch(const TEvent& event) {
    static_assert(std::is_base_of_v<Event, TEvent>, "TEvent must derive from Event");

    auto typeIndex = std::type_index(typeid(TEvent));
    auto it = handlers_.find(typeIndex);
    if (it == handlers_.end()) {
        return;
    }

    // ハンドラーリストのコピーを使って反復（配信中の購読変更に対応）
    auto handlersCopy = std::vector<IEventHandler*>();
    handlersCopy.reserve(it->second.size());
    for (auto& handler : it->second) {
        handlersCopy.push_back(handler.get());
    }

    for (auto* handler : handlersCopy) {
        handler->Invoke(event);
    }
}

template<typename TEvent, typename... Args>
void EventSystem::Queue(Args&&... args) {
    static_assert(std::is_base_of_v<Event, TEvent>, "TEvent must derive from Event");

    auto event = std::make_shared<TEvent>(std::forward<Args>(args)...);
    eventQueue_.push_back({std::type_index(typeid(TEvent)), std::move(event)});
}

template<typename TEvent>
void EventSystem::ClearEvent() {
    auto typeIndex = std::type_index(typeid(TEvent));
    auto it = handlers_.find(typeIndex);
    if (it != handlers_.end()) {
        // IDマッピングも削除
        for (auto& handler : it->second) {
            idToType_.erase(handler->GetId());
        }
        handlers_.erase(it);
    }
}

} // namespace UnoEngine
