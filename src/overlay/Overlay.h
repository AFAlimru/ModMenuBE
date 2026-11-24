#pragma once

#include <Windows.h>

struct ID3D11Device;
struct ID3D11ShaderResourceView;

namespace modmenu::overlay {

// Observe public DXGI factory calls on the initialization worker as early as
// possible, before slow image/settings work. No GPU device, Present/window
// hook, native call or input capture. Idempotent; uninstall cancels this phase.
bool observeSwapChainCreation();

// Enable Present/ResizeBuffers (D3D11On12 or plain D3D11) and WndProc only after
// public factory observation. Native admission is optional and independent.
// Observation must precede the game's swapchain creation;
// renderer installation may follow it. Failed renderer retries retain observers.
bool install();
void uninstall();

bool installed() noexcept;
HWND gameWindow();
// Also releases native-menu input ownership when a graphics failure latches.
bool graphicsFaulted() noexcept;

// Render thread only (inside ui frame callbacks).
ID3D11Device* device();
ID3D11ShaderResourceView* createTexture(void const* rgba, int width, int height);
void releaseTexture(ID3D11ShaderResourceView* texture);

} // namespace modmenu::overlay
