#pragma once
#include <cstdint>
#include <cstddef>
using UBaseType_t=unsigned;
struct Logger {template<class... T> void printf(const char*,T...){} void println(const char*){} };
static Logger Serial;
