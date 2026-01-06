#pragma once

#include <any>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <stdexcept>
#include <chrono>
#include <mutex>
#include <iostream>
#include <string>
#include <format>
#include <cstdint>
#include <vector>
#include <functional>
#include <fstream>

#include <windows.h>
#include <winioctl.h>

#include "Utility/Utility.hpp"

#ifndef NDEBUG
#define LOG( ... ) std::cout << std::format(__VA_ARGS__) << '\n'
#else
#define LOG( ... )
#endif
