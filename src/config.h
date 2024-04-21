// SPDX-License-Identifier: MulanPSL-2.0

/*
 * cmake config file
 * 创建于 2023年12月26日 上海市嘉定区安亭镇
 */

#pragma once

#include <string>
#include <cstdint>

// vesper 版本号
extern const std::string VESPER_VERSION_NAME;
extern const int64_t VESPER_VERSION_MAJOR;
extern const int64_t VESPER_VERSION_MINOR;
extern const int64_t VESPER_VERSION_PATCH;
extern const int64_t VESPER_VERSION_CODE;

/** example: 2024-04-21T06:00:52Z */
extern const std::string VESPER_BUILD_TIME_ISO8601;

/** example: April 21, 2024 14:00:52 CST */
extern const std::string VESPER_BUILD_TIME_HUMAN_READABLE;

