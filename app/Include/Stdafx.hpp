#pragma once

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

#include <any>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <stdexcept>
#include <chrono>
#include <mutex>
#include <thread>
#include <atomic>
#include <iostream>
#include <string>
#include <format>
#include <cstdint>
#include <vector>
#include <functional>
#include <fstream>
#include <ranges>

#include <windows.h>
#include <winioctl.h>
#include <wrl/client.h>
#include <d3d11.h>

#include "Utility/Utility.hpp"

#include "Common/StringPool.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#ifndef NDEBUG
#define LOG( ... ) std::cout << std::format(__VA_ARGS__) << '\n'
#else
#define LOG( ... )
#endif
