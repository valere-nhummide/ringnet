#pragma once

#include <cstdint>

namespace elio
{
enum class Operation : uint8_t { ACCEPT, CONNECT, READ, WRITE };
}
