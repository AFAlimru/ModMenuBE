#pragma once
#include "PassiveSettings.h"
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
namespace modmenu::compat {
class SettingsService {
    struct Task {
        std::uint64_t ticket;
        std::filesystem::path root;
        std::string id;
        bool apply{};
        menu::SettingsDraft draft;
    };
    struct Result {
        bool ready{};
        menu::SettingsDraft draft;
    };
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<Task> tasks_;
    std::map<std::uint64_t, Result> results_;
    bool stop_{};
    std::uint64_t next_{};
    std::thread worker_;
    void run();

  public:
    SettingsService();
    ~SettingsService();
    std::uint64_t load(std::filesystem::path, std::string);
    std::uint64_t apply(std::filesystem::path, menu::SettingsDraft);
    bool take(std::uint64_t, menu::SettingsDraft &);
    void discard(std::uint64_t);
};
SettingsService &settingsService();
} // namespace modmenu::compat
