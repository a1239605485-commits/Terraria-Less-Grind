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

#include "tool.hpp"

#include <fstream>
#include <iostream>
#include <regex>

// 平台架构定义
struct PlatformArch {
    const char *system_name; // 系统名
    const char *arch_name; // 架构名
    const char *lib_suffix; // 库文件后缀
    int file_id; // 固定文件ID
};

// 支持的平台架构组合和固定ID（从1开始）
static const PlatformArch SUPPORTED_PLATFORMS[] = {
    // Android
    {"android", "arm64", ".so", 1},
    {"android", "arm", ".so", 2},

    // Linux
    {"linux", "x64", ".so", 3},
    {"linux", "x86", ".so", 4},

    // Windows
    {"windows", "x64", ".dll", 5},
    {"windows", "x86", ".dll", 6},

    // macOS
    {"mac", "arm64", ".dylib", 7},
    {"mac", "x64", ".dylib", 8},

    // iOS
    {"ios", "arm64", ".dylib", 9},
    {"ios", "x64", ".dylib", 10},
    {"ios", "arm64-simulator", ".dylib", 11}, // 模拟器
};

// 包类型枚举
enum class PackageType {
    PLUGIN = 0, // 插件包
    LOADER = 1, // 加载器包
    MODULE = 2, // 模块包
    UNKNOWN = 3 // 未知类型
};

// 生成库文件名函数
static std::string generate_library_filename(const PackageType pkg_type, const PlatformArch &pa) {
    // 确定库类型前缀
    std::string type_prefix;
    switch (pkg_type) {
        case PackageType::PLUGIN: type_prefix = "plugin";
            break;
        case PackageType::LOADER: type_prefix = "loader";
            break;
        case PackageType::MODULE: type_prefix = "module";
            break;
        default: return ""; // 未知类型，返回空字符串
    }

    std::string filename = "lib" + type_prefix;

    // 添加系统、架构和后缀
    filename += '.';
    filename += pa.system_name;
    filename += '.';
    filename += pa.arch_name;
    filename += pa.lib_suffix;

    return filename;
}

// 检测包类型函数
static PackageType detect_package_type(const std::filesystem::path &dir) {
    // 检查目录中存在的库文件来确定包类型
    int plugin_count = 0;
    int loader_count = 0;
    int module_count = 0;

    for (const auto &pa: SUPPORTED_PLATFORMS) {
        // 检查每种类型
        for (int type = static_cast<int>(PackageType::PLUGIN);
             type <= static_cast<int>(PackageType::MODULE); type++) {
            const auto pkg_type = static_cast<PackageType>(type);
            std::string lib_filename = generate_library_filename(pkg_type, pa);

            if (lib_filename.empty()) continue;

            if (std::filesystem::path lib_path = dir / lib_filename;
                std::filesystem::exists(lib_path) && std::filesystem::is_regular_file(lib_path)) {
                switch (pkg_type) {
                    case PackageType::PLUGIN: plugin_count++;
                        break;
                    case PackageType::LOADER: loader_count++;
                        break;
                    case PackageType::MODULE: module_count++;
                        break;
                    default: break;
                }
            }
        }
    }

    // 根据存在最多的类型确定包类型
    if (plugin_count > 0 && loader_count == 0 && module_count == 0) {
        return PackageType::PLUGIN;
    }
    if (loader_count > 0 && plugin_count == 0 && module_count == 0) {
        return PackageType::LOADER;
    }
    if (module_count > 0 && plugin_count == 0 && loader_count == 0) {
        return PackageType::MODULE;
    }
    if (plugin_count > 0 && loader_count > 0) {
        std::cerr << "ERROR: Both plugin and loader files found in directory. Only one type is allowed." << std::endl;
        return PackageType::UNKNOWN;
    }
    if (plugin_count > 0 && module_count > 0) {
        std::cerr << "ERROR: Both plugin and module files found in directory. Only one type is allowed." << std::endl;
        return PackageType::UNKNOWN;
    }
    if (loader_count > 0 && module_count > 0) {
        std::cerr << "ERROR: Both loader and module files found in directory. Only one type is allowed." << std::endl;
        return PackageType::UNKNOWN;
    }

    return PackageType::UNKNOWN; // 没有找到任何库文件
}

// 检查是否至少存在一个库文件
static bool has_any_library(const std::filesystem::path &dir, const PackageType pkg_type) {
    for (const auto &pa: SUPPORTED_PLATFORMS) {
        std::string lib_filename = generate_library_filename(pkg_type, pa);
        if (lib_filename.empty()) continue;

        if (std::filesystem::path lib_path = dir / lib_filename;
            std::filesystem::exists(lib_path) && std::filesystem::is_regular_file(lib_path)) {
            return true;
        }
    }
    return false;
}

int generate_files_list(const std::filesystem::path &dir, const std::filesystem::path &out_file,
                         const std::vector<std::string_view> &exclude, int start_id) {
    std::cout << "INFO: Starting file list generation" << std::endl;
    std::cout << "INFO: Directory: " << dir << std::endl;
    std::cout << "INFO: Output file: " << out_file << std::endl;
    std::cout << "INFO: Start ID: " << start_id << std::endl;

    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        std::cerr << "ERROR: Directory not found: " << dir << std::endl;
        return start_id; // 返回起始ID
    }

    std::vector<std::pair<std::filesystem::path, int>> file_list;
    int current_id = start_id;

    std::cout << "INFO: Scanning directory..." << std::endl;

    for (const auto &entry: std::filesystem::recursive_directory_iterator(dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        std::filesystem::path relative_path = std::filesystem::relative(entry.path(), dir);
        std::string path_str = relative_path.generic_string();

        bool should_exclude = false;
        for (const auto &pattern: exclude) {
            if (path_str.find(pattern) != std::string::npos) {
                should_exclude = true;
                break;
            }
        }

        if (should_exclude) {
            std::cout << "DEBUG: Excluding file: " << path_str << std::endl;
            continue;
        }

        if (path_str.size() > 65535) {
            std::cerr << "WARNING: Path too long, skipping: " << path_str << std::endl;
            continue;
        }

        file_list.emplace_back(relative_path, current_id);
        current_id++;
    }

    std::sort(file_list.begin(), file_list.end(),
              [](const auto &a, const auto &b) {
                  return a.first < b.first;
              });

    std::cout << "INFO: Found " << file_list.size() << " files" << std::endl;

    if (file_list.empty()) {
        std::cout << "WARNING: No files found, but will generate empty list file" << std::endl;
        std::cout << "INFO: Next available ID: " << start_id << std::endl;
    } else {
        std::cout << "INFO: First file: " << file_list.front().first << " (ID: " << file_list.front().second << ")" <<
                std::endl;
        std::cout << "INFO: Last file: " << file_list.back().first << " (ID: " << file_list.back().second << ")" <<
                std::endl;
        std::cout << "INFO: Next available ID: " << current_id << std::endl; // 显示下一个可用ID
    }

    std::ofstream out_stream(out_file, std::ios::binary);
    if (!out_stream) {
        std::cerr << "ERROR: Cannot open output file: " << out_file << std::endl;
        return start_id; // 返回起始ID
    }

    std::cout << "INFO: Writing file list..." << std::endl;

    // Write entry count
    int entry_count = static_cast<int>(file_list.size());
    out_stream.write(reinterpret_cast<const char *>(&entry_count), sizeof(entry_count));

    // Write each entry
    for (const auto &[path, id]: file_list) {
        std::string path_str = path.generic_string();
        auto path_len = static_cast<uint16_t>(path_str.size());

        out_stream.write(reinterpret_cast<const char *>(&path_len), sizeof(path_len));
        out_stream.write(path_str.data(), path_len);
        out_stream.write(reinterpret_cast<const char *>(&id), sizeof(id));
    }

    out_stream.close();

    std::cout << "INFO: File list saved successfully" << std::endl;
    std::cout << "INFO: Output file size: " << std::filesystem::file_size(out_file) << " bytes" << std::endl;

    return current_id; // 返回下一个可用的ID
}

// 添加固定库文件函数
static int add_fixed_libraries(tefpkg_t *pkg,
                               const std::filesystem::path &dir,
                               const PackageType pkg_type,
                               const std::unordered_map<std::string, CompressConfig> &compress_map) {
    int added_count = 0;

    // 检查是否至少存在一个库文件
    if (!has_any_library(dir, pkg_type)) {
        std::cout << "INFO: No library files found for package type: "
                << (pkg_type == PackageType::PLUGIN ? "PLUGIN" : pkg_type == PackageType::LOADER ? "LOADER" : "MODULE")
                << ", skipping library addition" << std::endl;
        return 0;
    }

    std::cout << "INFO: Adding fixed libraries for package type: "
            << (pkg_type == PackageType::PLUGIN ? "PLUGIN" : pkg_type == PackageType::LOADER ? "LOADER" : "MODULE")
            << std::endl;

    // 为每个平台架构添加文件
    for (const auto &pa: SUPPORTED_PLATFORMS) {
        std::string lib_filename = generate_library_filename(pkg_type, pa);

        if (lib_filename.empty()) {
            continue;
        }

        if (std::filesystem::path lib_path = dir / lib_filename;
            std::filesystem::exists(lib_path) && std::filesystem::is_regular_file(lib_path)) {
            // 文件存在，正常添加
            CompressConfig compress_config = {COMPRESS_NONE, 0};
            if (compress_map.count(lib_filename) > 0) {
                compress_config = compress_map.at(lib_filename);
            }

            std::cout << "INFO: Adding library [" << pa.file_id << "]: "
                    << lib_filename << std::endl;

            const tefpkg_result_t result = tefpkg_add_entry_from_file(
                pkg, lib_path.string().c_str(),
                compress_config.type, compress_config.level);

            if (result != TEF_OK) {
                std::cerr << "ERROR: Failed to add library [" << pa.file_id
                        << "]: " << lib_filename << " (error: " << result << ")" << std::endl;
            } else {
                added_count++;
            }
        } else {
            // 文件不存在，添加空数据占位
            std::cout << "INFO: Adding empty placeholder [" << pa.file_id << "]: "
                    << lib_filename << std::endl;

            uint8_t empty_data[1] = {255};
            const tefpkg_result_t result = tefpkg_add_entry_from_memory(
                pkg, COMPRESS_NONE, 0, empty_data, 1);

            if (result != TEF_OK) {
                std::cerr << "ERROR: Failed to add empty placeholder [" << pa.file_id
                        << "]: " << lib_filename << " (error: " << result << ")" << std::endl;
            } else {
                added_count++;
            }
        }
    }

    return added_count;
}

// 通配符匹配函数
static bool wildcard_match(const std::string &pattern, const std::string &str) {
    try {
        // 将通配符模式转换为正则表达式
        std::string regex_str = std::regex_replace(pattern, std::regex("\\."), "\\.");
        regex_str = std::regex_replace(regex_str, std::regex("\\*"), ".*");
        regex_str = std::regex_replace(regex_str, std::regex("\\?"), ".");
        const auto wildcard_pattern = std::regex(regex_str, std::regex::icase);
        return std::regex_match(str, wildcard_pattern);
    } catch (const std::regex_error &e) {
        std::cerr << "ERROR: Invalid wildcard pattern: " << pattern << "\n" << e.what() << std::endl;
        return false;
    }
}

// 获取文件的压缩配置函数
static CompressConfig get_compress_config(const std::string &file_path,
                                          const std::unordered_map<std::string, CompressConfig> &compress_map) {
    // 1. 精确匹配完整路径
    if (compress_map.count(file_path) > 0) {
        return compress_map.at(file_path);
    }

    // 2. 通配符匹配
    for (const auto &[pattern, config]: compress_map) {
        if (pattern.find('*') != std::string::npos || pattern.find('?') != std::string::npos) {
            if (wildcard_match(pattern, file_path)) {
                return config;
            }
        }
    }

    // 3. 目录前缀匹配
    for (const auto &[pattern, config]: compress_map) {
        // 目录模式检查：以/结尾
        if (!pattern.empty() && pattern.back() == '/') {
            if (file_path.find(pattern) == 0) {
                // 以模式开头
                return config;
            }
        }
    }

    // 4. 扩展名匹配
    std::filesystem::path file_path_obj(file_path);
    if (std::string ext = file_path_obj.extension().string(); !ext.empty()) {
        // 尝试带点的扩展名
        if (compress_map.count(ext) > 0) {
            return compress_map.at(ext);
        }
        // 尝试不带点的扩展名
        else if (ext.length() > 1 && compress_map.count(ext.substr(1)) > 0) {
            return compress_map.at(ext.substr(1));
        }
    }

    // 5. 默认值
    return {COMPRESS_LZ4, 1};
}

// 完整 build_tefpkg 函数
void build_tefpkg(const std::filesystem::path &dir,
                  const std::filesystem::path &out_file,
                  uint64_t fingerprint,
                  bool files_list, // 控制是否生成文件列表
                  std::vector<std::string_view> exclude,
                  const std::unordered_map<std::string, CompressConfig> &compress_map) {
    const std::filesystem::path cache_dir = "Cache";
    if (!exists(cache_dir))
        std::filesystem::create_directories(cache_dir);

    const std::filesystem::path files_list_file = cache_dir / "file_list.bin";

    PackageType pkg_type = detect_package_type(dir);
    if (pkg_type == PackageType::UNKNOWN) {
        std::cout << "INFO: No package type detected, treating as generic package" << std::endl;
    } else {
        std::cout << "INFO: Detected package type: "
                << (pkg_type == PackageType::PLUGIN ? "PLUGIN" : pkg_type == PackageType::LOADER ? "LOADER" : "MODULE")
                << std::endl;
    }

    if (pkg_type != PackageType::UNKNOWN) {
        for (const auto &pa: SUPPORTED_PLATFORMS) {
            std::string lib_filename = generate_library_filename(pkg_type, pa);
            if (!lib_filename.empty()) {
                exclude.push_back(lib_filename);
                std::cout << "DEBUG: Added to exclude list: " << lib_filename << std::endl;
            }
        }
    }


    int start_id = pkg_type != PackageType::UNKNOWN ? 12 : 1;
    int reserved_entries = generate_files_list(dir, files_list_file, exclude, start_id);

    tefpkg_t *pkg = nullptr;
    tefpkg_result_t result = tefpkg_create_reserved_from_file(out_file.c_str(), reserved_entries, &pkg);

    if (result != TEF_OK) {
        std::cerr << "ERROR: Failed to create package: " << result << std::endl;
        return;
    }

    std::cout << "INFO: Package created successfully" << std::endl;

    std::cout << "INFO: Adding fixed file list (ID: 0)..." << std::endl;

    // 添加文件列表到包
    if (files_list) {
        result = tefpkg_add_entry_from_file(pkg, files_list_file.c_str(), COMPRESS_NONE, 0);
        if (result != TEF_OK) {
            std::cerr << "ERROR: Failed to add files list to package: " << result << std::endl;
            tefpkg_close(pkg);
            return;
        }
    } else {
        static std::vector<uint8_t> emtpy(1, 0);
        tefpkg_add_entry_from_memory(pkg, COMPRESS_NONE, 1, emtpy.data(), emtpy.size());
    }

    if (pkg_type != PackageType::UNKNOWN) {
        int libs_added = add_fixed_libraries(pkg, dir, pkg_type, compress_map);
        std::cout << "INFO: Added " << libs_added << " fixed libraries" << std::endl;
    }

    std::cout << "INFO: Adding user files..." << std::endl;

    // 读取文件列表
    std::ifstream list_stream(files_list_file, std::ios::binary);
    int entry_count = 0;
    list_stream.read(reinterpret_cast<char *>(&entry_count), sizeof(entry_count));

    int files_added = 0;
    int files_skipped = 0;

    for (int i = 0; i < entry_count; ++i) {
        uint16_t path_len = 0;
        list_stream.read(reinterpret_cast<char *>(&path_len), sizeof(path_len));

        if (path_len == 0) {
            std::cerr << "WARNING: Invalid path length in file list" << std::endl;
            continue;
        }

        std::string file_path(path_len, '\0');
        list_stream.read(file_path.data(), path_len);

        int file_id = 0;
        list_stream.read(reinterpret_cast<char *>(&file_id), sizeof(file_id));

        // 处理用户文件
        std::filesystem::path full_path = dir / file_path;

        if (!std::filesystem::exists(full_path)) {
            std::cerr << "WARNING: File not found, skipping: " << file_path << std::endl;
            files_skipped++;
            continue;
        }

        // 获取压缩配置
        CompressConfig compress_config = get_compress_config(file_path, compress_map);

        std::cout << "INFO: Adding user file [" << file_id << "]: " << file_path
                << " (compress: " << (compress_config.type == COMPRESS_NONE
                                          ? "NONE"
                                          : compress_config.type == COMPRESS_LZ4
                                                ? "LZ4"
                                                : "UNKNOWN")
                << ", level: " << compress_config.level << ")" << std::endl;

        result = tefpkg_add_entry_from_file(pkg, full_path.string().c_str(),
                                            compress_config.type, compress_config.level);

        if (result != TEF_OK) {
            std::cerr << "ERROR: Failed to add file [" << file_id << "]: " << file_path
                    << " (error: " << result << ")" << std::endl;
            files_skipped++;
        } else {
            files_added++;
        }
    }

    std::cout << "INFO: User files added: " << files_added
            << ", skipped: " << files_skipped << std::endl;

    // 后续的签名、验证、保存等代码
    std::cout << "INFO: Signing package with fingerprint..." << std::endl;
    result = tefpkg_sign_package(pkg, fingerprint);
    if (result != TEF_OK) {
        std::cerr << "ERROR: Failed to sign package: " << result << std::endl;
        tefpkg_close(pkg);
        return;
    }

    // 保存包
    std::cout << "INFO: Saving package to file..." << std::endl;
    result = tefpkg_save_file(pkg, fingerprint);
    if (result != TEF_OK) {
        std::cerr << "ERROR: Failed to save package: " << result << std::endl;
        tefpkg_close(pkg);
        return;
    }

    // 验证包完整性
    std::cout << "INFO: Verifying package integrity..." << std::endl;
    result = tefpkg_verify_pkg(pkg);
    if (result != TEF_OK) {
        std::cerr << "ERROR: Package integrity verification failed: " << result << std::endl;
        tefpkg_close(pkg);
        return;
    }

    // 验证签名
    std::cout << "INFO: Verifying package signature..." << std::endl;
    result = tefpkg_verify_signature(pkg, fingerprint);
    if (result != TEF_OK) {
        std::cerr << "ERROR: Package signature verification failed: " << result << std::endl;
        tefpkg_close(pkg);
        return;
    }

    // 获取最终包信息
    uint16_t total_entries = tefpkg_get_entries_count(pkg);
    std::cout << "INFO: Package created successfully!" << std::endl;
    std::cout << "INFO: Total entries in package: " << total_entries << std::endl;
    std::cout << "INFO: Package type: "
            << (pkg_type == PackageType::PLUGIN
                    ? "PLUGIN"
                    : pkg_type == PackageType::LOADER
                          ? "LOADER"
                          : pkg_type == PackageType::MODULE
                                ? "MODULE"
                                : "UNKNOWN")
            << std::endl;
    std::cout << "INFO: Output file: " << out_file << std::endl;

    if (std::filesystem::exists(out_file)) {
        std::cout << "INFO: Package size: " << std::filesystem::file_size(out_file) << " bytes" << std::endl;
    }

    // 清理
    tefpkg_close(pkg);

    // 删除临时文件
    std::filesystem::remove_all(cache_dir);

    std::cout << "INFO: Package build completed!" << std::endl;
}

// 其他辅助函数
void generate_key_file(const siphash_config_t &config, const std::filesystem::path &out_file) {
    siphash_make_key_to_file(out_file.c_str(), config);
}

uint64_t generate_fingerprint(const std::filesystem::path &key_file, const uint64_t seed) {
    siphash_keyfile_t *keyfile = nullptr;
    siphash_load_key_from_file(key_file.c_str(), &keyfile);
    const auto fingerprint = siphash_generate_fingerprint(seed, keyfile);
    free(keyfile);

    return fingerprint;
}
