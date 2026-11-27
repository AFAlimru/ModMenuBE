#include "ui/WorkspaceView.h"
#include "ui/LegacySettings.h"
#include "modmenu/ModMenuWorkspace.h"
#include <cstddef>
#ifndef _WIN32
static_assert(!modmenu::ui::workspaces::supported);
#else
static_assert(modmenu::ui::workspaces::supported);
#endif
static_assert(offsetof(ModMenuWorkspaceApi, version) == 0);
static_assert(offsetof(ModMenuWorkspaceApi, structSize) == 4);
// Compiled by the actual Android toolchain; no Workspace renderer or Windows
// backend is linked. The existing configuration route remains callable.
void workspacePlatformCompileContract(modmenu::ModInfo const& mod) {
    (void)modmenu::ui::workspaces::available(mod);
    (void)modmenu::ui::legacy::available(mod);
}
