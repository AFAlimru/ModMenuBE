#pragma once
#include "Settings.h"
#include <nlohmann/json.hpp>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
namespace menu {
using WorkspaceJson = nlohmann::json;
struct WorkspaceControl {
    std::string id, type, selectAction;
    SettingTranslation label, confirmation, emptyText;
    std::size_t minBytes{}, maxBytes{4096};
    bool identifier{};
};
struct WorkspacePage {
    std::string id;
    SettingTranslation label;
    std::vector<WorkspaceControl> controls;
};
struct WorkspaceDefinition {
    std::string id, entry, version;
    std::vector<WorkspacePage> pages;
    std::vector<std::string> externalTargets;
};
struct WorkspaceSnapshot {
    std::uint64_t session{};
    std::shared_ptr<WorkspaceDefinition const> definition;
    std::shared_ptr<WorkspaceJson const> state;
    std::vector<std::string> pending;
};
WorkspaceDefinition ParseWorkspace(std::string const&, std::string const& id, std::string const& entry,
                                   std::string const& version);
WorkspaceJson ParseWorkspaceState(std::string const&, WorkspaceDefinition const&);
WorkspaceJson StrictJson(std::string const&, std::size_t limit);
SettingTranslation WorkspaceCaption(WorkspaceJson const&, std::size_t limit = 160);
WorkspaceControl const* FindWorkspaceControl(WorkspaceDefinition const&, std::string const&);
class WorkspaceBroker {
    struct Provider {
        WorkspaceSnapshot snapshot;
        std::deque<std::string> commands;
        std::map<std::uint64_t, std::string> pending;
    };
    mutable std::mutex mutex_;
    std::map<std::uint64_t, Provider> providers_;
    std::uint64_t nextSession_{}, nextRequest_{};

  public:
    std::uint64_t Register(WorkspaceDefinition);
    bool Publish(std::uint64_t, std::string const&);
    bool Poll(std::uint64_t, char*, std::uint32_t capacity, std::uint32_t& required);
    bool Unregister(std::uint64_t);
    bool Submit(std::uint64_t, std::string const& page, std::string const& action, std::uint64_t revision,
                WorkspaceJson values, std::string const& row, std::string& error);
    std::vector<WorkspaceSnapshot> Snapshots() const;
};
WorkspaceBroker& Workspaces();
} // namespace menu
