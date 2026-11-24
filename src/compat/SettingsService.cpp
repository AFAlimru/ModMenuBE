#include "SettingsService.h"
namespace modmenu::compat {
SettingsService::SettingsService() : worker_([this] { run(); }) {}
SettingsService::~SettingsService() {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    condition_.notify_all();
    if (worker_.joinable())
        worker_.join();
}
void SettingsService::run() {
    for (;;) {
        Task t;
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, [&] { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty())
                return;
            t = std::move(tasks_.front());
            tasks_.pop_front();
        }
        menu::SettingsDraft result;
        try {
            result = t.apply ? menu::ApplySettings(t.root, t.draft) : menu::LoadSettings(t.root, t.id);
        } catch (...) {
            result.id = t.apply ? t.draft.id : t.id;
            result.message = "Settings operation failed.";
        }
        {
            std::lock_guard lock(mutex_);
            auto it = results_.find(t.ticket);
            if (it != results_.end())
                it->second = {true, std::move(result)};
        }
        condition_.notify_all();
    }
}
std::uint64_t SettingsService::load(std::filesystem::path root, std::string id) {
    std::lock_guard lock(mutex_);
    if (results_.size() >= 16 || tasks_.size() >= 16)
        return 0;
    auto ticket = ++next_;
    results_.emplace(ticket, Result{});
    tasks_.push_back({ticket, std::move(root), std::move(id), false, {}});
    condition_.notify_one();
    return ticket;
}
std::uint64_t SettingsService::apply(std::filesystem::path root, menu::SettingsDraft draft) {
    std::lock_guard lock(mutex_);
    if (results_.size() >= 16 || tasks_.size() >= 16)
        return 0;
    auto ticket = ++next_;
    results_.emplace(ticket, Result{});
    tasks_.push_back({ticket, std::move(root), {}, true, std::move(draft)});
    condition_.notify_one();
    return ticket;
}
bool SettingsService::take(std::uint64_t ticket, menu::SettingsDraft &out) {
    std::lock_guard lock(mutex_);
    auto it = results_.find(ticket);
    if (it == results_.end() || !it->second.ready)
        return false;
    out = std::move(it->second.draft);
    results_.erase(it);
    return true;
}
void SettingsService::discard(std::uint64_t ticket) {
    std::lock_guard lock(mutex_);
    results_.erase(ticket);
    std::erase_if(tasks_, [&](auto const &t) { return t.ticket == ticket && !t.apply; });
}
// The game DLL is pinned for process lifetime. Never join a worker under the
// loader lock during process detach; stack-owned services still join in tests.
SettingsService &settingsService() {
    static auto *service = new SettingsService;
    return *service;
}
} // namespace modmenu::compat
