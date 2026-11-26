add_rules("mode.debug", "mode.release")

set_project("ModMenuBE")
set_version("0.2.1")

add_requires("imgui v1.91.9", {configs = {shared = false, win32 = true, dx11 = true, freetype = false}})
add_requires("minhook", {configs = {shared = false}})
add_requires("nlohmann_json")

set_runtimes("MT")

-- Passive settings tests use the real parser and transactional file backend.
local passive_controls = {
    "src/compat/SettingsDescription.cpp", "src/compat/SettingsInput.cpp",
    "src/compat/SettingsSchema.cpp", "src/compat/Slider.cpp",
    "src/compat/TextEditor.cpp", "src/compat/Utf8.cpp"
}

for _, test in ipairs({"ExternalSettingsTests", "SettingsTransactionTests", "TextEditorTests", "SettingsFlowTests"}) do
    target(test)
        set_default(false)
        set_kind("binary")
        set_languages("c++20")
        add_includedirs("include", "src", "src/compat")
        add_files("tests/" .. test .. ".cpp")
        add_files(table.unpack(passive_controls))
        if test ~= "SettingsTransactionTests" then add_files("src/compat/Settings.cpp") end
        if test == "SettingsFlowTests" then add_files("src/compat/SettingsService.cpp") end
        add_packages("nlohmann_json")
        add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
        add_cxflags("/utf-8", "/EHsc")
end

target("SettingsCompatibilityTests")
    set_default(false)
    set_kind("binary")
    set_languages("c++20")
    add_includedirs("include", "src")
    add_files("tests/SettingsCompatibilityTests.cpp", "src/core/Settings.cpp", "src/core/Util.cpp", "src/core/Log.cpp")
    add_packages("nlohmann_json")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_syslinks("user32", "shell32")
    add_cxflags("/utf-8", "/EHsc")

target("SettingsUiTests")
    set_default(false)
    set_kind("binary")
    set_languages("c++20")
    add_includedirs("include", "src", "src/compat")
    add_files("tests/SettingsUiTests.cpp", "src/ui/OreTheme.cpp", "src/ui/SelfConfig.cpp",
              "src/core/ConfigRegistry.cpp", "src/core/I18n.cpp", "src/core/Settings.cpp",
              "src/core/Util.cpp", "src/core/Log.cpp", "src/compat/Settings.cpp",
              "src/compat/SettingsService.cpp")
    add_files(table.unpack(passive_controls))
    add_packages("imgui", "nlohmann_json")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_syslinks("user32", "shell32")
    add_cxflags("/utf-8", "/EHsc")

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
                 "d3d11", "d3d12", "dxgi", "imm32", "dwmapi")
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
