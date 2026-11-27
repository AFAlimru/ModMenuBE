#pragma once
#include "ModMenu/Workspace.h"
#include <optional>
namespace modmenu::workspace {
enum class HandoffPhase { None, Prepared, Queued, Delivered, Active, Returned, Failed };
struct Handoff {
    std::uint64_t session{}, request{}, deadline{};
    std::string target, message;
    HandoffPhase phase{};
};
class Service {
    mutable std::mutex mutex_;
    menu::WorkspaceBroker broker_;
    std::map<std::uint64_t, std::vector<std::string>> targets_;
    Handoff external_;
    std::uint64_t nextExternal_{1ull << 63};
    void expire(std::uint64_t now);

  public:
    std::uint64_t registerProvider(std::string const& raw, std::string const& id, std::string const& entry,
                                   std::string const& version);
    bool unregisterProvider(std::uint64_t session);
    bool publish(std::uint64_t session, std::string const& raw);
    bool poll(std::uint64_t session, char* output, std::uint32_t capacity, std::uint32_t& required,
              std::uint64_t now);
    std::optional<menu::WorkspaceSnapshot> snapshot(std::string const& id) const;
    bool hasExternal(std::uint64_t session, std::string const& target) const;
    bool submit(std::uint64_t session, std::string const& page, std::string const& action,
                std::uint64_t revision, menu::WorkspaceJson values, std::string const& row,
                std::string& error);
    std::uint64_t beginExternal(std::uint64_t session, std::string const& target, std::uint64_t now);
    bool releaseExternal(std::uint64_t session, std::uint64_t request, std::uint64_t now);
    bool acknowledge(std::uint64_t session, std::uint64_t request, bool success, std::uint64_t now);
    bool requestReturn(std::uint64_t session, std::uint64_t request, std::uint64_t now);
    Handoff handoff(std::uint64_t now);
    void cancelExternal(std::uint64_t request);
    void finishExternal(std::uint64_t request);
};
Service& service();
std::uint64_t milliseconds();
} // namespace modmenu::workspace
