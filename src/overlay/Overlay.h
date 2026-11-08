#pragma once

#include <Windows.h>

struct ID3D11Device;
struct ID3D11ShaderResourceView;

namespace modmenu::overlay {

// Hooks DXGI Present/ResizeBuffers (D3D12 through D3D11On12, or plain D3D11)
// and the game window's WndProc. Call from a worker thread once the game
// window exists. Safe to call again after a failure.
bool install();
void uninstall();

HWND gameWindow();

// Render thread only (inside ui frame callbacks).
ID3D11Device* device();
ID3D11ShaderResourceView* createTexture(void const* rgba, int width, int height);
void releaseTexture(ID3D11ShaderResourceView* texture);

} // namespace modmenu::overlay
