#pragma once

#include "core/ModRegistry.h"

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace modmenu {

struct UpdateResult {
    enum class State { Unknown, Checking, UpToDate, Available, NeedsNewerGame, Error, NoSource } state = State::Unknown;
    std::string latest;      // newest version usable with this game
    std::string newestAny;   // newest version overall (may need a newer game)
    std::string url;         // page to open
    std::string error;
};

class UpdateChecker {
public:
    static UpdateChecker& instance();

    // Checks every mod with an update source on a background thread.
    // A second call while a check runs is ignored.
    void checkAll();
    bool running() const { return running_; }
    UpdateResult result(std::string const& mod) const;
    int availableCount() const;

private:
    void run(std::shared_ptr<ModList const> mods);
    UpdateResult check(ModInfo const& mod);

    mutable std::mutex                  mutex_;
    std::map<std::string, UpdateResult> results_;
    std::atomic<bool>                   running_{false};
};

} // namespace modmenu
