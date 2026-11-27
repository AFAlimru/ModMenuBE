/* Optional workspace API v1. MIT. UTF-8 buffers copied within each call.
 * Query from an already loaded ModMenuBE.dll; never call from DllMain.
 * Provider worker owns registration, state publication, command execution,
 * external UI acknowledgement and explicit return. No foreign callbacks.
 * 0 success; 1 Poll needs larger buffer; -1 invalid argument;
 * -2 overlay service not ready; -3 identity/session/data refused.
 * Byte counts exclude NUL on input; Poll required includes NUL.
 * No implicit actions, command replay, task cancellation on page close or
 * compatibility with ModMenuBedrock.dll's old export names is implied.
 */
#ifndef MODMENU_WORKSPACE_H
#define MODMENU_WORKSPACE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MODMENU_WORKSPACE_VERSION 1u
/* Independent from ModMenuApi; it does not change API v1/v2.
 * Query uses a MINIMUM supported version: 0 or 1 accepts the current v1 table;
 * a future minimum returns NULL. On Android this optional query returns NULL.
 * version and structSize are the stable prefix. New members are append-only:
 * require version >= the needed version and structSize >= the end of every
 * member used; never assume equality, read a future member, or copy structSize
 * bytes into a locally compiled smaller structure. */
typedef struct ModMenuWorkspaceApi {
    uint32_t version, structSize;
    int32_t (*isReady)(void);
    int32_t (*registerProvider)(const void* ownerAddress, const char* declaration, uint32_t bytes,
                                uint64_t* session);
    int32_t (*publish)(uint64_t session, const char* state, uint32_t bytes);
    int32_t (*poll)(uint64_t session, char* output, uint32_t capacity, uint32_t* required);
    int32_t (*unregisterProvider)(uint64_t session);
    /* Call after the requested target UI was created (1), or creation failed (0).
     * Only a delivered openExternal request can be acknowledged, once. */
    int32_t (*acknowledgeExternal)(uint64_t session, uint64_t requestId, int32_t success);
    /* Explicit user Back/Return action in the external UI. Closing a window or
     * losing focus must not call this implicitly. Origin/session revalidated. */
    int32_t (*returnToWorkspace)(uint64_t session, uint64_t requestId);
} ModMenuWorkspaceApi;
typedef const ModMenuWorkspaceApi* (*ModMenu_GetWorkspaceApiFn)(uint32_t minVersion);
#if defined(MODMENU_BUILDING)
#if defined(_WIN32)
__declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif
const ModMenuWorkspaceApi*
ModMenu_GetWorkspaceApi(uint32_t minVersion);
#endif
#ifdef __cplusplus
}
#endif
#endif
