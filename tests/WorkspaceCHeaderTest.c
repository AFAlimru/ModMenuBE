#include "modmenu/ModMenuWorkspace.h"
#include <stddef.h>
_Static_assert(offsetof(ModMenuWorkspaceApi, isReady) == 8, "stable C prefix");
_Static_assert(sizeof(ModMenuWorkspaceApi) == 8 + 7 * sizeof(void (*)(void)), "C table layout");
int workspaceCHeaderSize(void) { return sizeof(ModMenuWorkspaceApi); }
