#include "workspace/Service.h"
#include <iostream>
int main() {
    using namespace modmenu::workspace;
    int failures = 0;
    auto check = [&](bool v, char const* t) {
        if (!v) {
            ++failures;
            std::cerr << "FAIL " << t << '\n';
        }
    };
    Service service;
    std::string declaration =
        R"({"schemaVersion":1,"plugin":{"id":"Example","entry":"Example.dll","versions":["1"]},"pages":[],"externalTargets":["editor","viewer"]})";
    auto session = service.registerProvider(declaration, "Example", "Example.dll", "1");
    check(session != 0, "external-only provider allowed");
    auto snapshot = service.snapshot("Example");
    check(snapshot && snapshot->session == session && service.hasExternal(session, "editor"),
          "targets bound to provider");
    std::uint32_t n = 0;
    check(service.poll(session, nullptr, 0, n, 0) && n == 0, "registration does not enqueue");
    auto request = service.beginExternal(session, "editor", 100);
    check(request != 0, "explicit external preparation");
    check(service.poll(session, nullptr, 0, n, 100) && n == 0, "input not released means no command");
    check(!service.releaseExternal(session, request + 1, 101), "wrong request cannot release");
    check(service.releaseExternal(session, request, 101), "UI ownership release permits command");
    check(service.poll(session, nullptr, 0, n, 102) && n > 0, "size query retains handoff");
    auto needed = n;
    char shortBuf[2]{};
    check(service.poll(session, shortBuf, 2, n, 103) && n == needed, "short handoff buffer does not dequeue");
    std::vector<char> bytes(n);
    check(service.poll(session, bytes.data(), n, n, 104), "copy external command");
    auto command = menu::WorkspaceJson::parse(bytes.data());
    check(command["kind"] == "openExternal" && command["target"] == "editor" &&
              command["requestId"] == request,
          "literal external target");
    check(!service.acknowledge(session + 1, request, true, 105), "foreign session cannot acknowledge");
    check(service.acknowledge(session, request, true, 105), "explicit success acknowledgement");
    check(!service.acknowledge(session, request, true, 106), "ack cannot replay");
    check(service.requestReturn(session, request, 107), "explicit return accepted");
    check(!service.requestReturn(session, request, 108), "return cannot replay");
    check(service.handoff(108).phase == HandoffPhase::Returned, "return visible to UI");
    service.finishExternal(request);
    check(!service.beginExternal(session, "unknown", 200), "unknown target refused");
    auto timeout = service.beginExternal(session, "viewer", 200);
    check(timeout != 0, "second external target");
    service.releaseExternal(session, timeout, 201);
    check(service.handoff(15202).phase == HandoffPhase::Failed, "acknowledgement times out");
    check(service.poll(session, nullptr, 0, n, 15203) && n == 0, "expired command not delivered later");
    check(!service.acknowledge(session, timeout, true, 15204), "late ack rejected");
    service.finishExternal(timeout);
    auto cancel = service.beginExternal(session, "editor", 16000);
    service.cancelExternal(cancel);
    check(service.handoff(16001).phase == HandoffPhase::Failed, "scene cancellation invalidates handoff");
    service.finishExternal(cancel);
    check(service.unregisterProvider(session) && !service.snapshot("Example"), "unregister removes provider");
    check(!service.requestReturn(session, request, 17000), "stale session refused");
    std::cout << "workspace handoff failures=" << failures << '\n';
    return failures ? 1 : 0;
}
