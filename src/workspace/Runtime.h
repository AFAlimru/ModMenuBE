#pragma once
#include <cstdint>
#include <string>
namespace modmenu::workspace {
std::int32_t registerLoadedProvider(void const* owner,char const* raw,std::uint32_t size,std::uint64_t* session);
}
