#pragma once

#include <cstddef>

// Returns this process's current working-set size in bytes, or 0 if unavailable.
std::size_t processWorkingSetBytes();
