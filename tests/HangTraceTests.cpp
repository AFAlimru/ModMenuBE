#include "overlay/HangTrace.h"
#include "overlay/OpenDiagnostics.h"
#include "core/Paths.h"
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <thread>

using namespace modmenu::overlay::diagnostics;
int main() {
    int checks = 0, failures = 0;
    auto check = [&](bool ok, char const* why) {
        ++checks;
        if (!ok) {
            ++failures;
            std::cerr << "FAIL " << why << '\n';
        }
    };
    HangTrace trace;
    check(!trace.poll(Channel::Render, 99999, 3000), "idle must never report a hang");
    trace.enter(Channel::Render, Stage::Win32Frame, 1000, 42);
    check(!trace.poll(Channel::Render, 3999, 3000), "short operation must not report");
    auto report = trace.poll(Channel::Render, 4000, 3000);
    check(report && report->stage == Stage::Win32Frame && report->elapsed == 3000 && report->thread == 42,
          "unchanged operation must identify the exact stage and thread at timeout");
    check(!trace.poll(Channel::Render, 8000, 3000), "unchanged stalled operation reports only once");
    trace.enter(Channel::Render, Stage::DrawGpu, 9000, 42);
    check(!trace.poll(Channel::Render, 9001, 3000), "progress restarts timeout");
    report = trace.poll(Channel::Render, 12000, 3000);
    check(report && report->stage == Stage::DrawGpu && report->elapsed == 3000,
          "next stalled stage must be observable");
    trace.clear(Channel::Render);
    check(!trace.poll(Channel::Render, 18000, 3000), "completed frame cannot become a false hang");
    trace.enter(Channel::Request, Stage::AwaitFrame, 20000, 51);
    trace.enter(Channel::Window, Stage::WindowOriginal, 21000, 52);
    check(!trace.poll(Channel::Window, 23999, 3000), "window timeout is independent");
    report = trace.poll(Channel::Request, 24000, 3000);
    check(report && report->thread == 51 && report->elapsed == 4000,
          "queued open without rendering must report");
    report = trace.poll(Channel::Window, 24000, 3000);
    check(report && report->thread == 52 && report->stage == Stage::WindowOriginal,
          "window blockage reports separately");
    trace.clear(Channel::Window);
    trace.enter(Channel::Window, Stage::WindowOriginal, 30000, 52);
    check(bool(trace.poll(Channel::Window, 33000, 3000)), "same stage in a later operation can report again");
    trace.enter(Channel::Render, Stage::Present, 40000, 42);
    check(!trace.poll(Channel::Render, 39999, 3000), "older observer timestamp cannot underflow into a hang");
    check(bool(trace.poll(Channel::Render, 43000, 3000)),
          "future observer reports the original Present blockage");
    trace.clear(Channel::Render);
    std::thread producer([&] {
        for (unsigned i = 0; i < 100000; ++i) {
            trace.enter(Channel::Render, Stage::DrawGpu, 50000 + i, 99);
            trace.clear(Channel::Render);
        }
    });
    bool valid = true;
    for (unsigned i = 0; i < 100000; ++i)
        if (auto r = trace.poll(Channel::Render, 160000, 3000))
            valid &=
                r->stage == Stage::DrawGpu && r->thread == 99 && r->elapsed >= 3000 && r->elapsed <= 110000;
    producer.join();
    check(valid, "concurrent publication must not produce mixed stage or idle records");
    check(!trace.poll(Channel::Render, 180000, 3000), "producer completion clears the active stage");
    auto logDir = modmenu::paths::selfDir() / L"logs";
    std::filesystem::create_directories(logDir);
    start();
    auto read = [](std::filesystem::path const& path) {
        std::ifstream in(path);
        return std::string(std::istreambuf_iterator<char>(in), {});
    };
    auto diagnostic = logDir / L"open-diagnostic.log";
    {
        Scope parent(Channel::Render, Stage::DrawGpu);
        { Scope nested(Channel::Render, Stage::Present); }
        // A second producer must not overwrite an in-flight render operation.
        std::thread other([] {
            Scope contender(Channel::Render, Stage::Win32Frame);
            contender.stage(Stage::FontTexture);
        });
        other.join();
        auto deadline = GetTickCount64() + 6000;
        while (GetTickCount64() < deadline && read(diagnostic).find("STALL") == std::string::npos)
            Sleep(50);
        auto text = read(diagnostic);
        check(text.find("stage=DrawGpu") != std::string::npos,
              "independent watchdog observes stalled outer operation after nested scope");
        check(text.find("stage=Win32Frame") == std::string::npos &&
                  text.find("stage=FontTexture") == std::string::npos,
              "concurrent producer cannot corrupt recorded ownership");
        Sleep(500);
        text = read(diagnostic);
        auto first = text.find("STALL");
        check(first != std::string::npos && text.find("STALL", first + 1) == std::string::npos,
              "actual worker does not flood unchanged stall records");
    }
    Scope next(Channel::Render, Stage::Flush);
    check(read(diagnostic).find("diagnostic monitor") != std::string::npos,
          "dedicated diagnostic file has initialized header");
    std::cout << checks << " checks / " << failures << " failures\n";
    return failures ? 1 : 0;
}
