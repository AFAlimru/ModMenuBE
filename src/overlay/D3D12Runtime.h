#pragma once

#include <d3d12.h>

namespace modmenu::overlay {

// Worker thread only, outside DllMain. Keeps the system runtime loaded for the
// process lifetime; never binds the SDK's ordinal import to an app-local shim.
PFN_D3D12_CREATE_DEVICE systemD3D12CreateDevice() noexcept;

} // namespace modmenu::overlay
