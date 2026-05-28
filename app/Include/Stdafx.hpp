#pragma once

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

#include <memory>
#include <stdexcept>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <atomic>
#include <iostream>
#include <string>
#include <format>
#include <cstdint>
#include <vector>
#include <fstream>
#include <ranges>
#include <concepts>
#include <memory_resource>
#include <algorithm>
#include <numeric>
#include <execution>
#include <any>
#include <typeindex>
#include <unordered_map>
#include <functional>

#include <windows.h>
#include <winioctl.h>
#include <wrl/client.h>
#include <d3d11.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "Common/StringPool.h"
#include "Utility/RAII.hpp"
#include "Utility/Utility.hpp"
#include "Searcher/FileDatabase.hpp"
#include "Searcher/Searcher.hpp"
#include "UI/Renderer.hpp"
#include "UI/ResultsPage.hpp"
#include "UI/Window.hpp"
#include "Application/Application.hpp"

#ifndef NDEBUG
#define LOG( ... ) std::cout << std::format(__VA_ARGS__) << '\n'
#else
#define LOG( ... )
#endif
