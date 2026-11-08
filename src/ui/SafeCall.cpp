#include "ui/SafeCall.h"

#include <Windows.h>

// No C++ objects with destructors may live in these functions (SEH + C++ unwinding).
namespace modmenu::ui::safe {

namespace {
int filter(unsigned code) {
    // Let the debugger / crash logger see breakpoints and stack overflows untouched.
    return code == EXCEPTION_BREAKPOINT || code == EXCEPTION_STACK_OVERFLOW ? EXCEPTION_CONTINUE_SEARCH
                                                                             : EXCEPTION_EXECUTE_HANDLER;
}
} // namespace

bool getBool(ConfigOption const& o, bool& out) {
    __try {
        out = o.getBool(o.user) != 0;
        return true;
    } __except (filter(GetExceptionCode())) {
        return false;
    }
}

bool setBool(ConfigOption const& o, bool v) {
    __try {
        o.setBool(o.user, v ? 1 : 0);
        return true;
    } __except (filter(GetExceptionCode())) {
        return false;
    }
}

bool getInt(ConfigOption const& o, int64_t& out) {
    __try {
        out = o.getInt(o.user);
        return true;
    } __except (filter(GetExceptionCode())) {
        return false;
    }
}

bool setInt(ConfigOption const& o, int64_t v) {
    __try {
        o.setInt(o.user, v);
        return true;
    } __except (filter(GetExceptionCode())) {
        return false;
    }
}

bool getFloat(ConfigOption const& o, double& out) {
    __try {
        out = o.getFloat(o.user);
        return true;
    } __except (filter(GetExceptionCode())) {
        return false;
    }
}

bool setFloat(ConfigOption const& o, double v) {
    __try {
        o.setFloat(o.user, v);
        return true;
    } __except (filter(GetExceptionCode())) {
        return false;
    }
}

bool getString(ConfigOption const& o, char* out, uint32_t capacity) {
    __try {
        if (capacity) out[0] = 0;
        o.getString(o.user, out, capacity);
        if (capacity) out[capacity - 1] = 0;
        return true;
    } __except (filter(GetExceptionCode())) {
        return false;
    }
}

bool setString(ConfigOption const& o, char const* v) {
    __try {
        o.setString(o.user, v);
        return true;
    } __except (filter(GetExceptionCode())) {
        return false;
    }
}

bool action(ModMenuActionFn fn, void* user) {
    if (!fn) return true;
    __try {
        fn(user);
        return true;
    } __except (filter(GetExceptionCode())) {
        return false;
    }
}

} // namespace modmenu::ui::safe
