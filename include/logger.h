#pragma once

#include "spdlog/spdlog.h"

extern std::shared_ptr<spdlog::logger> GlobalLogger;

void init_global_logger();
void set_log_level(spdlog::level::level_enum log_level);