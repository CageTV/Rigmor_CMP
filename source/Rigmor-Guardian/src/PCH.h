#pragma once

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <nlohmann/json.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

using namespace std::literals;
namespace logger = SKSE::log;
