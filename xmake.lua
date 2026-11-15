add_rules("mode.debug", "mode.release")

set_project("ModMenuBE")
set_version("0.2.0-local3")

add_requires("imgui v1.91.9", {configs = {shared = false, win32 = true, dx11 = true, freetype = false}})
add_requires("minhook", {configs = {shared = false}})
add_requires("nlohmann_json")

set_runtimes("MT")

target("ModMenuBE")
    set_kind("shared")
    set_languages("c++20")
    set_encodings("utf-8")
    add_defines("NOMINMAX", "UNICODE", "_UNICODE", "WIN32_LEAN_AND_MEAN", "MODMENU_BUILDING")
    add_includedirs("include", "src", "src/compat")
    add_files("src/**.cpp")
    add_headerfiles("include/(modmenu/*.h)", "include/(modmenu/*.hpp)")
    add_packages("imgui", "minhook", "nlohmann_json")
    add_syslinks("user32", "gdi32", "shell32", "ole32", "windowscodecs", "winhttp",
                 "d3d11", "dxgi", "imm32", "dwmapi", "bcrypt")
    set_symbols("debug")
    set_optimize("faster")

    after_build(function (target)
        local out = path.join("bin", "ModMenuBE")
        os.mkdir(out)
        os.cp(target:targetfile(), out)
        os.cp("manifest.json", out)
        os.cp("LICENSE", out)
        os.cp("THIRD_PARTY_NOTICES.txt", out)
        os.cp("assets/icon.png", out)
        os.rm(path.join(out, "adapters"))
        os.cp("adapters", out)
    end)

-- Development only: a D3D12 window that loads the mod (xmake build TestHost).
target("TestHost")
    set_default(false)
    set_kind("binary")
    set_languages("c++20")
    set_encodings("utf-8")
    add_defines("NOMINMAX", "UNICODE", "_UNICODE")
    add_files("tests/TestHost.cpp")
    add_syslinks("user32", "d3d12", "dxgi")
    add_ldflags("/SUBSYSTEM:CONSOLE", {force = true})

target("OverlayPreview")
    set_default(false)
    set_kind("shared")
    set_languages("c++20")
    set_encodings("utf-8")
    add_defines("NOMINMAX", "UNICODE", "_UNICODE", "WIN32_LEAN_AND_MEAN", "MODMENU_BUILDING")
    add_includedirs("include", "src", "src/compat")
    add_files("src/**.cpp", "tests/PreviewBridge.cpp")
    remove_files("src/compat/NativeBridge.cpp")
    add_packages("imgui", "minhook", "nlohmann_json")
    add_syslinks("user32", "gdi32", "shell32", "ole32", "windowscodecs", "winhttp", "d3d11", "dxgi", "imm32", "dwmapi", "bcrypt")

target("D3D12RuntimeTests")
    set_default(false)
    set_kind("binary")
    set_languages("c++20")
    add_defines("NOMINMAX", "UNICODE", "_UNICODE", "WIN32_LEAN_AND_MEAN")
    add_includedirs("src")
    add_files("tests/D3D12RuntimeTests.cpp", "src/overlay/D3D12Runtime.cpp")
    add_cxflags("/utf-8", "/EHsc")

target("LocalIntegrationTests")
    set_default(false)
    set_kind("binary")
    set_languages("c++20")
    add_defines("NOMINMAX", "UNICODE", "_UNICODE", "WIN32_LEAN_AND_MEAN")
    add_includedirs("src", "include", "src/compat")
    add_files("tests/LocalIntegrationTests.cpp", "src/compat/Settings*.cpp", "src/compat/Slider.cpp", "src/compat/TextEditor.cpp", "src/compat/Utf8.cpp")
    add_packages("nlohmann_json")
    add_cxflags("/utf-8", "/EHsc")

for _, spec in ipairs({
    {"DescriptorInventory", {"src/compat/Settings*.cpp", "src/compat/Slider.cpp", "src/compat/TextEditor.cpp", "src/compat/Utf8.cpp"}},
    {"DefinitionTests", {"src/compat/Definition.cpp"}},
    {"NativeTests", {"src/compat/NativeProfile.cpp"}},
    {"Native51VariantsTests", {"src/compat/NativeProfile.cpp"}},
    {"ExternalSettingsTests", {"src/compat/Settings*.cpp", "src/compat/Slider.cpp", "src/compat/TextEditor.cpp", "src/compat/Utf8.cpp"}},
    {"SettingsTransactionTests", {"src/compat/SettingsDescription.cpp", "src/compat/SettingsSchema.cpp", "src/compat/SettingsInput.cpp", "src/compat/Slider.cpp", "src/compat/TextEditor.cpp", "src/compat/Utf8.cpp"}},
    {"TextEditorTests", {"src/compat/Settings*.cpp", "src/compat/Slider.cpp", "src/compat/TextEditor.cpp", "src/compat/Utf8.cpp"}}
}) do
    target(spec[1])
        set_default(false)
        set_kind("binary")
        set_languages("c++20")
        add_defines("NOMINMAX", "UNICODE", "_UNICODE", "WIN32_LEAN_AND_MEAN")
        add_includedirs("include", "src", "src/compat")
        add_files("tests/" .. spec[1] .. ".cpp")
        add_files(table.unpack(spec[2]))
        add_packages("nlohmann_json")
        add_syslinks("bcrypt")
        add_cxflags("/utf-8", "/EHsc")
end
