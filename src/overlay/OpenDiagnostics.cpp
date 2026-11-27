#include "overlay/OpenDiagnostics.h"
#include "core/Paths.h"
#include <Windows.h>
#include <cstdio>

namespace modmenu::overlay::diagnostics {
namespace {
HangTrace trace;
std::atomic<bool> enabled{};
std::array<std::atomic<DWORD>, unsigned(Channel::Count)> owners{};
struct LocalStage {
    Stage stage{};
    std::uint64_t at{};
    unsigned depth{};
};
thread_local std::array<LocalStage, unsigned(Channel::Count)> local;
char const* name(Stage stage) {
    switch (stage) {
#define NAME(value)                                                                                          \
    case Stage::value:                                                                                       \
        return #value
        NAME(Idle);
        NAME(AwaitFrame);
        NAME(RenderLock);
        NAME(GraphicsInit);
        NAME(BackBuffers);
        NAME(PrepareUi);
        NAME(FontTexture);
        NAME(Messages);
        NAME(Win32Frame);
        NAME(ImGuiFrame);
        NAME(DrawUi);
        NAME(BuildDrawData);
        NAME(AcquireWrapped);
        NAME(BindTarget);
        NAME(DrawGpu);
        NAME(ReleaseWrapped);
        NAME(Flush);
        NAME(Present);
        NAME(WindowDispatch);
        NAME(WindowInput);
        NAME(WindowOriginal);
        NAME(BufferCleanup);
        NAME(GpuCompletion);
#undef NAME
    }
    return "Unknown";
}
DWORD WINAPI monitor(void* handle) {
    auto file = static_cast<HANDLE>(handle);
    constexpr char header[] = "ModMenuBE diagnostic monitor; timeout=3000ms; record limit=64.\r\n"
                              "Records identify a stalled boundary, not its cause. No input text, "
                              "credentials or memory dumps are collected.\r\n";
    DWORD written{};
    WriteFile(file, header, sizeof(header) - 1, &written, nullptr);
    FlushFileBuffers(file);
    unsigned records = 0;
    while (enabled.load() && records < 64) {
        Sleep(250);
        auto now = GetTickCount64();
        for (unsigned i = 0; i < unsigned(Channel::Count) && records < 64; ++i) {
            if (auto sample = trace.poll(Channel(i), now, 3000)) {
                SYSTEMTIME time{};
                GetLocalTime(&time);
                char line[256]{};
                auto n = std::snprintf(
                    line, sizeof(line),
                    "%02u:%02u:%02u.%03u STALL channel=%u stage=%s elapsed_ms=%llu thread=%lu\r\n",
                    time.wHour, time.wMinute, time.wSecond, time.wMilliseconds, i, name(sample->stage),
                    static_cast<unsigned long long>(sample->elapsed),
                    static_cast<unsigned long>(sample->thread));
                if (n > 0 && n < int(sizeof(line))) {
                    WriteFile(file, line, DWORD(n), &written, nullptr);
                    FlushFileBuffers(file);
                }
                ++records;
            }
        }
    }
    enabled = false;
    CloseHandle(file);
    return 0;
}
} // namespace

void start() {
    static std::atomic<bool> started{};
    if (started.exchange(true))
        return;
    auto path = paths::selfDir() / L"logs" / L"open-diagnostic.log";
    auto file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;
    enabled = true;
    auto worker = CreateThread(nullptr, 0, monitor, file, 0, nullptr);
    if (worker)
        CloseHandle(worker);
    else {
        enabled = false;
        CloseHandle(file);
    }
}
void enter(Channel channel, Stage stage) noexcept {
    if (!enabled.load())
        return;
    auto thread = GetCurrentThreadId();
    if (channel != Channel::Request && owners[unsigned(channel)].load() != thread)
        return;
    auto& current = local[unsigned(channel)];
    current.stage = stage;
    current.at = GetTickCount64();
    trace.enter(channel, stage, current.at, thread);
}
void clear(Channel channel) noexcept {
    if (!enabled.load())
        return;
    if (channel != Channel::Request && owners[unsigned(channel)].load() != GetCurrentThreadId())
        return;
    trace.clear(channel);
}
Scope::Scope(Channel channel, Stage stage) noexcept : channel_(channel) {
    if (!enabled.load())
        return;
    auto thread = GetCurrentThreadId();
    auto& owner = owners[unsigned(channel)];
    DWORD empty{};
    if (owner.load() != thread && !owner.compare_exchange_strong(empty, thread))
        return;
    active_ = true;
    auto& current = local[unsigned(channel)];
    outer_ = current.depth++ == 0;
    previous_ = current.stage;
    previousAt_ = current.at;
    enter(channel, stage);
}
Scope::~Scope() {
    if (!active_)
        return;
    auto& current = local[unsigned(channel_)];
    --current.depth;
    if (outer_) {
        clear(channel_);
        current = {};
        owners[unsigned(channel_)] = 0;
    } else {
        current.stage = previous_;
        current.at = previousAt_;
        trace.enter(channel_, previous_, previousAt_, GetCurrentThreadId());
    }
}
} // namespace modmenu::overlay::diagnostics
