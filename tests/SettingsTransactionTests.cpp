// Exercise the actual private transaction cleanup, without exporting test APIs.
#include "../src/compat/Settings.cpp"
#include <fstream>
void TransactionChecks(void (*check)(bool, char const *)) {
    namespace fs = std::filesystem;
    auto root =
        fs::temp_directory_path() / (L"ModMenuTransactionTests-" + std::to_wstring(GetCurrentProcessId()) +
                                     L"-" + std::to_wstring(GetTickCount64()));
    fs::create_directory(root);
    struct Cleanup {
        fs::path p;
        ~Cleanup() {
            std::error_code ec;
            fs::remove_all(p, ec);
        }
    } cleanup{root};
    auto path = root / "preexisting.tmp";
    std::ofstream(path) << "do not delete";
    bool refused = false;
    try {
        menu::WriteNew(path, "replacement");
    } catch (...) {
        refused = true;
    }
    menu::CleanupTemporary(path, false);
    check(refused && fs::exists(path), "failed CREATE_NEW cannot clean up a pre-existing transaction path");
    auto owned = root / "owned.tmp";
    menu::WriteNew(owned, "draft");
    menu::CleanupTemporary(owned, true);
    check(!fs::exists(owned), "failed save removes only its successfully created temporary file");
    auto directory = root / "Example";
    fs::create_directory(directory);
    auto manifest = directory / "manifest.json", description = directory / "modmenu.settings.json";
    std::ofstream(manifest) << R"({"entry":"Example.dll","version":"1"})";
    std::ofstream(description)
        << R"({"schemaVersion":1,"plugin":{"id":"Example","entry":"Example.dll","versions":["1"]},"config":{"file":"config.json","reload":"hot"},"fields":[{"path":"/enabled","type":"boolean","default":true,"label":"Enabled"}]})";
    {
        menu::Sources sources(directory, "Example");
        auto write = CreateFileW(description.c_str(), GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                 OPEN_EXISTING, 0, nullptr);
        check(write == INVALID_HANDLE_VALUE,
              "authoritative description handle blocks concurrent writes throughout transaction");
        if (write != INVALID_HANDLE_VALUE)
            CloseHandle(write);
        check(!MoveFileExW(manifest.c_str(), (directory / "manifest.old").c_str(), 0),
              "authoritative manifest handle blocks concurrent rename throughout transaction");
    }
    auto write = CreateFileW(description.c_str(), GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                             0, nullptr);
    check(write != INVALID_HANDLE_VALUE, "authoritative source locks release after transaction scope");
    if (write != INVALID_HANDLE_VALUE)
        CloseHandle(write);
}

#include <iostream>
static int failures{};
int main() {
    TransactionChecks([](bool ok, char const *message) {
        if (!ok) {
            ++failures;
            std::cerr << message << '\n';
        }
    });
    std::cout << "Transaction cleanup failures: " << failures << '\n';
    return failures ? 1 : 0;
}
