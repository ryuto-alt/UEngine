#include "pch.h"
#include "EventSystem.h"
#include "Logger.h"

namespace UnoEngine {

EventSystem& EventSystem::GetInstance() {
    static EventSystem instance;
    return instance;
}

void EventSystem::Unsubscribe(EventCallbackId id) {
    auto typeIt = idToType_.find(id);
    if (typeIt == idToType_.end()) {
        return;
    }

    auto& typeIndex = typeIt->second;
    auto handlersIt = handlers_.find(typeIndex);
    if (handlersIt != handlers_.end()) {
        auto& handlerList = handlersIt->second;
        handlerList.erase(
            std::remove_if(handlerList.begin(), handlerList.end(),
                [id](const std::unique_ptr<IEventHandler>& handler) {
                    return handler->GetId() == id;
                }),
            handlerList.end()
        );

        // 空になったハンドラーリストを削除
        if (handlerList.empty()) {
            handlers_.erase(handlersIt);
        }
    }

    idToType_.erase(typeIt);
}

void EventSystem::ProcessQueue() {
    // キューをスワップして処理（処理中に新しいイベントがキューされる可能性に対応）
    auto queue = std::move(eventQueue_);
    eventQueue_.clear();

    for (auto& queued : queue) {
        auto it = handlers_.find(queued.type);
        if (it == handlers_.end()) {
            continue;
        }

        // ハンドラーリストのコピーを使って反復
        auto handlersCopy = std::vector<IEventHandler*>();
        handlersCopy.reserve(it->second.size());
        for (auto& handler : it->second) {
            handlersCopy.push_back(handler.get());
        }

        for (auto* handler : handlersCopy) {
            handler->Invoke(*queued.event);
        }
    }
}

void EventSystem::ClearAll() {
    handlers_.clear();
    idToType_.clear();
    eventQueue_.clear();
    Logger::Debug("[EventSystem] All subscriptions and queued events cleared");
}

size_t EventSystem::GetSubscriberCount() const {
    size_t count = 0;
    for (auto& [type, handlerList] : handlers_) {
        count += handlerList.size();
    }
    return count;
}

EventCallbackId EventSystem::GenerateId() {
    return nextId_++;
}

} // namespace UnoEngine
