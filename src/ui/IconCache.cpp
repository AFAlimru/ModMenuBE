#include "ui/IconCache.h"

#include "core/Log.h"
#include "core/Util.h"
#include "overlay/Overlay.h"

#include <Windows.h>
#include <wincodec.h>

#include <d3d11.h>

#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace modmenu::ui::icons {

namespace {

struct Entry {
    enum class State { Decoding, Decoded, Ready, Failed } state = State::Decoding;
    std::vector<uint8_t>       rgba;
    int                        width = 0, height = 0;
    ID3D11ShaderResourceView*  srv{};
};

std::mutex                              gMutex;
std::map<std::wstring, Entry>           gEntries;

constexpr UINT kMaxSide = 128;

bool decode(std::filesystem::path const& path, std::vector<uint8_t>& out, int& w, int& h) {
    HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool    ok = false;
    IWICImagingFactory*    factory{};
    IWICBitmapDecoder*     decoder{};
    IWICBitmapFrameDecode* frame{};
    IWICBitmapScaler*      scaler{};
    IWICFormatConverter*   converter{};
    do {
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
            break;
        if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                      WICDecodeMetadataCacheOnDemand, &decoder)))
            break;
        if (FAILED(decoder->GetFrame(0, &frame))) break;
        UINT fw = 0, fh = 0;
        frame->GetSize(&fw, &fh);
        if (fw == 0 || fh == 0) break;
        IWICBitmapSource* source = frame;
        // Shrink large icons; pixel-art icons (<=128) keep nearest-neighbour sharpness.
        if (fw > kMaxSide || fh > kMaxSide) {
            float k  = float(kMaxSide) / float(fw > fh ? fw : fh);
            UINT  nw = UINT(fw * k) ? UINT(fw * k) : 1, nh = UINT(fh * k) ? UINT(fh * k) : 1;
            if (FAILED(factory->CreateBitmapScaler(&scaler))) break;
            if (FAILED(scaler->Initialize(frame, nw, nh, WICBitmapInterpolationModeFant))) break;
            source = scaler;
            fw     = nw;
            fh     = nh;
        }
        if (FAILED(factory->CreateFormatConverter(&converter))) break;
        if (FAILED(converter->Initialize(source, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0,
                                         WICBitmapPaletteTypeCustom)))
            break;
        out.resize(size_t(fw) * fh * 4);
        if (FAILED(converter->CopyPixels(nullptr, fw * 4, UINT(out.size()), out.data()))) break;
        w  = int(fw);
        h  = int(fh);
        ok = true;
    } while (false);
    if (converter) converter->Release();
    if (scaler) scaler->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (factory) factory->Release();
    if (SUCCEEDED(co)) CoUninitialize();
    return ok;
}

} // namespace

ImTextureID get(std::filesystem::path const& png) {
    if (png.empty()) return ImTextureID{};
    std::wstring key = png.wstring();
    std::lock_guard lock(gMutex);
    auto [it, inserted] = gEntries.try_emplace(key);
    if (inserted) {
        std::thread([key, png] {
            Entry result;
            bool  ok = decode(png, result.rgba, result.width, result.height);
            std::lock_guard l(gMutex);
            auto found = gEntries.find(key);
            if (found == gEntries.end()) return;
            if (!ok) {
                found->second.state = Entry::State::Failed;
                log::warn("could not decode icon {}", pathUtf8(png));
                return;
            }
            found->second.rgba   = std::move(result.rgba);
            found->second.width  = result.width;
            found->second.height = result.height;
            found->second.state  = Entry::State::Decoded;
        }).detach();
    }
    return it->second.state == Entry::State::Ready ? reinterpret_cast<ImTextureID>(it->second.srv) : ImTextureID{};
}

void pump() {
    std::lock_guard lock(gMutex);
    for (auto& [_, e] : gEntries) {
        if (e.state != Entry::State::Decoded) continue;
        e.srv   = overlay::createTexture(e.rgba.data(), e.width, e.height);
        e.state = e.srv ? Entry::State::Ready : Entry::State::Failed;
        e.rgba.clear();
        e.rgba.shrink_to_fit();
    }
}

void clear() {
    std::lock_guard lock(gMutex);
    for (auto& [_, e] : gEntries) overlay::releaseTexture(e.srv);
    gEntries.clear();
}

} // namespace modmenu::ui::icons
