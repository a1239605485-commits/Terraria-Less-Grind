/*******************************************************************************
 * tefpkg_tool - tool
 * Copyright (C) 2026 eternalfuture-e38299
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * Author: eternalfuture-e38299
 * GitHub: https://github.com/eternalfuture-e38299
 * Created: 2026/3/7
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <unordered_map>
#include <vector>

#include "libs/tefpkg.h"
#include "libs/siphash.h"

struct CompressConfig {
    tefpkg_compress_t type = COMPRESS_LZ4;
    uint8_t level = 1;  // 压缩等级
};

int generate_files_list(const std::filesystem::path &dir, const std::filesystem::path &out_file,
                         const std::vector<std::string_view> &exclude = {}, int start_id = 2);

void generate_key_file(const siphash_config_t& config, const std::filesystem::path &out_file);

uint64_t generate_fingerprint(const std::filesystem::path &key_file, uint64_t seed = 0);

void build_tefpkg(const std::filesystem::path &dir,
    const std::filesystem::path &out_file,
    uint64_t fingerprint,
    bool files_list = false,
    std::vector<std::string_view> exclude = {},
    const std::unordered_map<std::string, CompressConfig> &compress_map = {}
);