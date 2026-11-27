#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>

namespace modmenu::overlay::diagnostics {

enum class Channel : unsigned { Request, Render, Window, Count };
enum class Stage : std::uint16_t {
    Idle,
    AwaitFrame,
    RenderLock,
    GraphicsInit,
    BackBuffers,
    PrepareUi,
    FontTexture,
    Messages,
    Win32Frame,
    ImGuiFrame,
    DrawUi,
    BuildDrawData,
    AcquireWrapped,
    BindTarget,
    DrawGpu,
    ReleaseWrapped,
    Flush,
    Present,
    WindowDispatch,
    WindowInput,
    WindowOriginal,
    BufferCleanup,
    GpuCompletion
};
struct Sample {
    Stage stage{};
    std::uint64_t elapsed{};
    std::uint32_t thread{};
};

// Callbacks publish numbers only. One observer polls outside render/window locks.
class HangTrace {
  public:
    void enter(Channel channel, Stage stage, std::uint64_t now, std::uint32_t thread) noexcept {
        auto& slot = slots_[unsigned(channel)];
        auto version = slot.version.load();
        // Never wait in a callback, including if another producer was preempted.
        if ((version & 1) || !slot.version.compare_exchange_strong(version, version + 1))
            return;
        slot.time.store(now);
        slot.detail.store((std::uint64_t(thread) << 16) | unsigned(stage));
        slot.version.store(version + 2);
    }
    void clear(Channel channel) noexcept {
        enter(channel, Stage::Idle, 0, 0);
    }
    std::optional<Sample> poll(Channel channel, std::uint64_t now, std::uint64_t timeout) noexcept {
        auto& slot = slots_[unsigned(channel)];
        auto version = slot.version.load();
        if ((version & 1) || version == slot.reported)
            return {};
        auto time = slot.time.load(), detail = slot.detail.load();
        if (slot.version.load() != version)
            return {};
        auto stage = Stage(detail & 0xffff);
        if (stage == Stage::Idle || now < time || now - time < timeout)
            return {};
        slot.reported = version;
        return Sample{stage, now - time, std::uint32_t(detail >> 16)};
    }

  private:
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    struct Slot {
        std::atomic<std::uint64_t> version{}, time{}, detail{};
        std::uint64_t reported{}; // single watchdog consumer only
    };
    std::array<Slot, unsigned(Channel::Count)> slots_{};
};

} // namespace modmenu::overlay::diagnostics
