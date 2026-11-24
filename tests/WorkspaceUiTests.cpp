// Actual workspace renderer/input, offscreen only. No game, OS window or DLL load.
#include "../src/ui/WorkspaceView.cpp"
#include "core/Settings.h"
#include <d3d11.h>
#include <imgui_impl_dx11.h>
#include <fstream>
#include <filesystem>
#include <iostream>

namespace modmenu::compat {
bool nativeReady() {
    return false;
}
std::string gameLanguage() {
    return "en_US";
}
} // namespace modmenu::compat
int main(int argc, char** argv) {
    using namespace modmenu;
    namespace ui = modmenu::ui::workspaces;
    namespace ws = modmenu::workspace;
    int failures = 0;
    auto check = [&](bool ok, char const* why) {
        if (!ok) {
            ++failures;
            std::cerr << "FAIL " << why << '\n';
        }
    };
    std::string declaration =
        R"({"schemaVersion":1,"plugin":{"id":"UiExample","entry":"Example.dll","versions":["1"]},"externalTargets":["editor"],"pages":[{"id":"jobs","label":"Tasks","controls":[{"id":"start","type":"action","label":{"en_US":"Start demo task","zh_TW":"啟動示範任務"}},{"id":"name","type":"text","label":{"en_US":"Task name","zh_TW":"任務名稱"},"minBytes":0,"maxBytes":128},{"id":"flag","type":"boolean","label":{"en_US":"Example option","zh_TW":"示範選項"}},{"id":"status","type":"status","label":{"en_US":"Worker status","zh_TW":"工作狀態"}},{"id":"progress","type":"progress","label":{"en_US":"Task progress","zh_TW":"任務進度"}},{"id":"targets","type":"list","label":{"en_US":"Targets","zh_TW":"目標"},"selectAction":"choose"}]},{"id":"map","label":"Preview","controls":[{"id":"image","type":"image","label":{"en_US":"Bounded pixel preview","zh_TW":"有界像素預覽"}}]}]})";
    auto session = ws::service().registerProvider(declaration, "UiExample", "Example.dll", "1");
    check(session != 0, "register actual broker fixture");
    menu::WorkspaceJson state = {
        {"protocolVersion", 1},
        {"revision", 1},
        {"controls",
         {{"start", {{"enabled", true}}},
          {"name", {{"value", "Example"}}},
          {"flag", {{"value", false}}},
          {"status", {{"text", "No commands have run. / 尚未執行操作"}}},
          {"progress", {{"value", 25}, {"total", 100}}},
          {"targets",
           {{"rows", menu::WorkspaceJson::array(
                         {{{"id", "one"}, {"label", "First target"}, {"text", "Available"}},
                          {{"id", "two"}, {"label", "Second target"}, {"text", "Available"}}})}}},
          {"image", {{"width", 32}, {"height", 24}, {"pixels", menu::WorkspaceJson::array()}}}}}};
    for (unsigned y = 0; y < 24; ++y)
        for (unsigned x = 0; x < 32; ++x)
            state["controls"]["image"]["pixels"].push_back((x / 4 + y / 4) % 2 ? 0x3C8527FFu : 0xD0D1D4FFu);
    check(ws::service().publish(session, state.dump()), "publish actual state");
    ModInfo mod;
    mod.name = "UiExample";
    mod.loaded = true;
    auto adapter = std::make_shared<Adapter>();
    adapter->workspace = ws::parseEntry(
        R"({"workspace":{"version":1,"entries":[{"id":"tasks","type":"page","target":"jobs","label":{"en_US":"Tasks","zh_TW":"任務"}},{"id":"preview","type":"page","target":"map","label":{"en_US":"Preview","zh_TW":"預覽"}},{"id":"editor","type":"external","target":"editor","label":{"en_US":"Open editor","zh_TW":"開啟編輯器"}},{"id":"missing","type":"page","target":"missing","label":"Unavailable target"}]}})");
    mod.adapter = adapter;
    ModInfo plain;
    plain.loaded = true;
    check(!ui::available(plain), "ordinary plugin has no workspace");
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DeltaTime = 1.f / 60;
    io.DisplaySize = {900, 760};
    settings().setLanguage("en_US");
    reloadLanguage();
    modmenu::ui::fonts::note(declaration);
    modmenu::ui::fonts::update(1);
    modmenu::ui::ore::applyStyle(1);
    ID3D11Device* device{};
    ID3D11DeviceContext* context{};
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                                 &device, nullptr, &context)))
        return 2;
    ImGui_ImplDX11_Init(device, context);
    auto frame = [&]() {
        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("workspace-offscreen", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        ui::draw(io.DisplaySize.y - 24);
        ImGui::End();
        ImGui::Render();
    };
    auto child = [&](char const* part) -> ImGuiWindow* {
        for (auto* w : GImGui->Windows)
            if (std::string(w->Name).find(part) != std::string::npos)
                return w;
        return nullptr;
    };
    auto click = [&](ImVec2 p) {
        io.AddMousePosEvent(p.x, p.y);
        frame();
        io.AddMouseButtonEvent(0, true);
        frame();
        io.AddMouseButtonEvent(0, false);
        frame();
    };
    auto capture = [&](char const* name) {
        if (argc < 2)
            return;
        std::filesystem::create_directories(argv[1]);
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = UINT(io.DisplaySize.x);
        desc.Height = UINT(io.DisplaySize.y);
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        ID3D11Texture2D* texture{};
        check(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &texture)), "offscreen target");
        if (!texture)
            return;
        ID3D11RenderTargetView* view{};
        device->CreateRenderTargetView(texture, nullptr, &view);
        context->OMSetRenderTargets(1, &view, nullptr);
        float clear[] = {.19f, .2f, .2f, 1};
        context->ClearRenderTargetView(view, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ID3D11Texture2D* stage{};
        device->CreateTexture2D(&desc, nullptr, &stage);
        context->CopyResource(stage, texture);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(context->Map(stage, 0, D3D11_MAP_READ, 0, &mapped))) {
            std::ofstream out(std::filesystem::path(argv[1]) / (std::string(name) + ".ppm"),
                              std::ios::binary);
            out << "P6\n" << desc.Width << ' ' << desc.Height << "\n255\n";
            for (UINT y = 0; y < desc.Height; ++y) {
                auto* p = static_cast<unsigned char*>(mapped.pData) + y * mapped.RowPitch;
                for (UINT x = 0; x < desc.Width; ++x)
                    out.write(reinterpret_cast<char*>(p + x * 4), 3);
            }
            context->Unmap(stage, 0);
        } else
            check(false, "readback");
        stage->Release();
        view->Release();
        texture->Release();
    };
    check(ui::open(mod), "open declared loaded plugin");
    frame();
    frame();
    capture("hub-en");
    std::uint32_t required{};
    check(ws::service().poll(session, nullptr, 0, required, ws::milliseconds()) && required == 0,
          "passive render enqueues nothing");
    auto hub = child("workspace-entry-hub");
    check(hub != nullptr, "actual hub child exists");
    if (hub)
        click({hub->DC.CursorStartPos.x + 60, hub->DC.CursorStartPos.y + 22});
    check(ui::model.page == "jobs", "actual pointer click opens page");
    frame();
    capture("jobs-en");
    auto jobs = child("workspace-page-jobs");
    check(jobs != nullptr, "actual jobs child exists");
    if (jobs)
        click({jobs->DC.CursorStartPos.x + 65, jobs->DC.CursorStartPos.y + ImGui::GetFontSize() + 8 + 20});
    check(ws::service().poll(session, nullptr, 0, required, ws::milliseconds()) && required > 0,
          "actual action click enqueues command");
    if (required) {
        std::vector<char> bytes(required);
        ws::service().poll(session, bytes.data(), required, required, ws::milliseconds());
        auto command = menu::WorkspaceJson::parse(bytes.data());
        check(command["action"] == "start" && command["values"]["name"] == "Example",
              "actual form reaches broker");
    }
    frame();
    frame();
    if (jobs)
        click({jobs->DC.CursorStartPos.x + 65, jobs->DC.CursorStartPos.y + ImGui::GetFontSize() + 8 + 20});
    check(ws::service().poll(session, nullptr, 0, required, ws::milliseconds()) && required == 0,
          "pending action and redraw never replay click");
    ui::model.page = "map";
    frame();
    frame();
    capture("map-en");
    settings().setLanguage("zh_TW");
    reloadLanguage();
    modmenu::ui::fonts::update(1);
    ImGui_ImplDX11_InvalidateDeviceObjects();
    frame();
    frame();
    capture("map-zh");
    io.DisplaySize = {420, 700};
    ui::model.page = "jobs";
    frame();
    frame();
    capture("jobs-zh-narrow");
    io.DisplaySize = {900, 900};
    modmenu::ui::fonts::update(1.5);
    ImGui_ImplDX11_InvalidateDeviceObjects();
    modmenu::ui::ore::applyStyle(1.5);
    frame();
    frame();
    capture("jobs-zh-scale150");
    ui::model.scroll["jobs"] = 24;
    ui::model.text["name"] = "Committed";
    ui::model.committed["name"] = "Committed";
    ui::model.editing = "name";
    ui::model.text["name"] = "Unfinished";
    ui::suspend();
    check(ui::model.text["name"] == "Committed" && ui::model.page == "jobs",
          "actual blur retains page, cancels unfinished edit");
    auto request = ws::service().beginExternal(session, "editor", ws::milliseconds());
    ui::tick(true, true, false);
    check(ui::inputSuspended() && !ui::externalActive(), "held input delays external ownership");
    check(ws::service().poll(session, nullptr, 0, required, ws::milliseconds()) && required == 0,
          "held input cannot deliver external command");
    ui::tick(true, true, true);
    check(ui::externalActive(), "release relinquishes ModMenu input");
    ws::service().poll(session, nullptr, 0, required, ws::milliseconds());
    std::vector<char> command(required);
    check(required > 0, "released handoff reaches provider");
    if (required)
        ws::service().poll(session, command.data(), required, required, ws::milliseconds());
    check(ws::service().acknowledge(session, request, true, ws::milliseconds()),
          "acknowledge actual UI handoff");
    check(ws::service().requestReturn(session, request, ws::milliseconds()), "explicit provider return");
    ui::tick(true, true, true);
    check(!ui::inputSuspended() && ui::model.page == "jobs" && ui::model.scroll["jobs"] == 24,
          "explicit return retains original page and scroll");
    check(!ui::tick(false, true, true) && ui::model.mod.empty(), "changed origin discards stale workspace");
    ui::close();
    ws::service().unregisterProvider(session);
    ImGui_ImplDX11_Shutdown();
    context->Release();
    device->Release();
    ImGui::DestroyContext();
    std::cout << "workspace actual UI failures=" << failures << '\n';
    return failures ? 1 : 0;
}
