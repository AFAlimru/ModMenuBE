#pragma once
#include "overlay/HangTrace.h"

namespace modmenu::overlay::diagnostics {
// Start once from the installation worker, before enabling DXGI hooks.
// No log I/O or waiting occurs in these stage publication calls.
void start();
void enter(Channel, Stage) noexcept;
void clear(Channel) noexcept;

class Scope {
  public:
    Scope(Channel channel, Stage stage) noexcept;
    ~Scope();
    Scope(Scope const&) = delete;
    Scope& operator=(Scope const&) = delete;
    void stage(Stage stage) noexcept {
        enter(channel_, stage);
    }

  private:
    Channel channel_;
    bool active_{}, outer_{};
    Stage previous_{};
    std::uint64_t previousAt_{};
};
} // namespace modmenu::overlay::diagnostics
