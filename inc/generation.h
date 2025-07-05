#pragma once
#include "position.h"
#include "settings.h"
#include "utils.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <thread>

#ifdef __cpp_lib_format
#include <format>
#endif

constexpr int startLimit = 500;
constexpr int verifyDepth = 10;
constexpr int softLim = 5000;
constexpr int hardLim = 500000;
constexpr int depthLim = 20;
constexpr int randomPlyNorm = 2;
constexpr int randomPlyDFRC = 4;
constexpr int minPlySave = 16;

constexpr int drawThreshold = 5;
constexpr int drawPlies = 15;
constexpr int winThreshold = 2000;
constexpr int winPlies = 5;

enum class genLaunch {
    Ask,
    Normal,
    DFRC
};

void mergeFiles();
void startGen(const genLaunch mode);