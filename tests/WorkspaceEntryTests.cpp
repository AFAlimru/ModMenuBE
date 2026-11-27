#include "workspace/Entry.h"
#include <iostream>
int main() {
    using namespace modmenu::workspace;
    int failures = 0;
    auto check = [&](bool v, char const* m) {
        if (!v) {
            ++failures;
            std::cerr << "FAIL " << m << '\n';
        }
    };
    check(!parseEntry("{}").visible(), "absent hides entry");
    check(!parseEntry(R"({"workspace":false})").visible(), "false hides entry");
    auto good = parseEntry(
        R"({"config":false,"workspace":{"version":1,"label":{"en_US":"Workspace","zh_CN":"工作页"},"entries":[{"id":"jobs","type":"page","target":"jobs","label":"Tasks"},{"id":"editor","type":"external","target":"editor_ui","label":"Open editor"}]}})");
    check(good.visible() && good.entries.size() == 2, "page and external declaration visible");
    check(good.label.get("zh_CN") == "工作页", "localized label");
    check(!parseEntry(R"({"workspace":{"version":2,"entries":[]}})").visible(), "unknown version refused");
    check(
        !parseEntry(
             R"({"workspace":{"version":1,"entries":[{"id":"a","type":"external","target":"../bad","label":"Bad"}]}})")
             .visible(),
        "unsafe target refused");
    check(
        !parseEntry(
             R"({"workspace":{"version":1,"entries":[{"id":"a","type":"page","target":"jobs","label":"A"},{"id":"a","type":"external","target":"editor","label":"B"}]}})")
             .visible(),
        "duplicate entry ID refused");
    check(
        !parseEntry(
             R"({"workspace":{"version":1,"version":1,"entries":[{"id":"a","type":"page","target":"jobs","label":"A"}]}})")
             .visible(),
        "duplicate JSON member refused");
    check(
        !parseEntry(
             R"({"workspace":{"version":1,"entries":[{"id":"a","type":"shell","target":"jobs","label":"A"}]}})")
             .visible(),
        "unknown type refused");
    check(!parseEntry(R"({"workspace":true})").visible(), "boolean true not capability");
    check(
        parseEntry(
            R"({/* legacy comments */"workspace":{"version":1,"entries":[{"id":"a","type":"external","target":"ui","label":"UI"}]}})")
            .visible(),
        "comment-compatible descriptor");
    check(!good.entries[0].external && good.entries[1].external, "typed targets");
    std::cout << "workspace entry failures=" << failures << '\n';
    return failures ? 1 : 0;
}
