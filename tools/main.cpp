#include <algorithm>
#include <fstream>
#include <iostream>
#include <iomanip>

#include "json.hpp"
#include "tool.hpp"

// 从JSON文件读取压缩配置
std::unordered_map<std::string, CompressConfig> load_compress_config_from_json(const std::filesystem::path& json_file) {
    std::unordered_map<std::string, CompressConfig> compress_map;

    if (!std::filesystem::exists(json_file)) {
        std::cerr << "WARNING: Compress config JSON file not found: " << json_file << std::endl;
        return compress_map;
    }

    try {
        std::ifstream file(json_file);
        nlohmann::json j = nlohmann::json::parse(file);

        for (const auto& [pattern, config] : j.items()) {
            if (config.is_object()) {
                CompressConfig compress_config;

                if (config.contains("mode")) {
                    auto mode_str = config["mode"].get<std::string>();
                    std::transform(mode_str.begin(), mode_str.end(), mode_str.begin(), ::tolower);

                    if (mode_str == "none") {
                        compress_config.type = COMPRESS_NONE;
                    } else if (mode_str == "lz4") {
                        compress_config.type = COMPRESS_LZ4;
                    } else if (mode_str == "lz4hc") {
                        compress_config.type = COMPRESS_LZ4HC;
                    } else {
                        std::cerr << "WARNING: Unknown compress mode: " << mode_str
                                  << " for pattern: " << pattern << std::endl;
                        continue;
                    }
                } else {
                    std::cerr << "WARNING: Missing 'mode' field for pattern: " << pattern << std::endl;
                    continue;
                }

                if (config.contains("level")) {
                    compress_config.level = config["level"].get<uint8_t>();
                } else {
                    compress_config.level = 1;  // 默认压缩等级
                }

                compress_map[pattern] = compress_config;

                std::cout << "INFO: Loaded compress config: " << pattern
                          << " -> mode: " << static_cast<int>(compress_config.type)
                          << ", level: " << static_cast<int>(compress_config.level) << std::endl;
            }
        }

        std::cout << "INFO: Loaded " << compress_map.size() << " compress configs from "
                  << json_file << std::endl;

    } catch (const nlohmann::json::exception& e) {
        std::cerr << "ERROR: Failed to parse compress config JSON: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Failed to load compress config: " << e.what() << std::endl;
    }

    return compress_map;
}

// 打印目录中的文件列表
void print_directory_files(const std::filesystem::path& dir,
                          const std::vector<std::string_view>& exclude = {},
                          const int start_id = 2) {
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        std::cerr << "ERROR: Directory not found: " << dir << std::endl;
        return;
    }

    std::vector<std::pair<std::filesystem::path, int>> file_list;
    int current_id = start_id;

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

    std::cout << "File list for directory: " << dir << std::endl;
    std::cout << "Total files: " << file_list.size() << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    std::cout << std::left << std::setw(6) << "ID"
              << std::left << std::setw(50) << "Path"
              << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    for (const auto& [path, id] : file_list) {
        std::cout << std::left << std::setw(6) << id
                  << std::left << std::setw(50) << path.string() << std::endl;
    }

    if (file_list.empty()) {
        std::cout << "No files found" << std::endl;
    }
}

// 将目录文件列表转换为C宏定义
void convert_directory_to_c_macro(const std::filesystem::path& dir,
                                 const std::filesystem::path& output_file,
                                 const std::vector<std::string_view>& exclude = {},
                                 int start_id = 2) {
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        std::cerr << "ERROR: Directory not found: " << dir << std::endl;
        return;
    }

    std::vector<std::pair<std::filesystem::path, int>> file_list;
    int current_id = start_id;

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

    std::ofstream out_stream(output_file);
    if (!out_stream) {
        std::cerr << "ERROR: Cannot open output file: " << output_file << std::endl;
        return;
    }

    out_stream << "/**\n";
    out_stream << " * Auto-generated file ID macros\n";
    out_stream << " * Generated from directory: " << dir.string() << "\n";
    out_stream << " * DO NOT EDIT THIS FILE DIRECTLY\n";
    out_stream << " */\n\n";
    out_stream << "#pragma once\n\n";

    for (const auto& [path, id] : file_list) {
        std::string file_path = path.generic_string();

        // 转换路径为有效的宏名
        std::string macro_name = "FILE_ID_";
        for (char c : file_path) {
            if (std::isalnum(c)) {
                macro_name += std::to_string(std::toupper(c));
            } else
                macro_name.push_back('_');
        }

        // 去除重复的下划线
        std::string clean_macro_name;
        bool last_was_underscore = false;
        for (char c : macro_name) {
            if (c == '_') {
                if (!last_was_underscore) {
                    clean_macro_name += c;
                    last_was_underscore = true;
                }
            } else {
                clean_macro_name += c;
                last_was_underscore = false;
            }
        }

        // 去除末尾的下划线
        if (!clean_macro_name.empty() && clean_macro_name.back() == '_') {
            clean_macro_name.pop_back();
        }

        out_stream << "#define " << std::left << std::setw(60) << clean_macro_name
                   << " " << id << "  // " << file_path << std::endl;
    }

    out_stream << "\n// Total files: " << file_list.size() << std::endl;

    out_stream.close();

    std::cout << "INFO: Generated C macro file: " << output_file << std::endl;
    std::cout << "INFO: Total macros: " << file_list.size() << std::endl;
}

// 主命令行处理函数
void handle_command_line(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "TEFPKG Tool - Package Builder Tool\n";
        std::cout << "Version: 1.0.0\n";
        std::cout << "Usage: " << argv[0] << " <command> [options]\n\n";
        std::cout << "Commands:\n";
        std::cout << "  list <dir> [exclude...]           List files in directory\n";
        std::cout << "  tomacro <dir> <output.h> [exclude...] Convert to C macros\n";
        std::cout << "  genkey <author> <org> <loc> <output> Generate SipHash key\n";
        std::cout << "  fingerprint <key_file> [seed]     Generate fingerprint\n";
        std::cout << "  build <dir> <output> [fingerprint] Build package\n";
        std::cout << "\nBuild Options:\n";
        std::cout << "  -e, --exclude <pattern>    Exclude pattern (can be multiple)\n";
        std::cout << "  -c, --compress <json>      Compression config JSON file\n";
        std::cout << "  -n, --no-file-list         Do not generate file list (default: generate)\n";
        std::cout << "\nExamples:\n";
        std::cout << "  " << argv[0] << " list ./src \"*.tmp\" \"cache/*\"\n";
        std::cout << "  " << argv[0] << " tomacro ./src ./file_ids.h \"*.tmp\"\n";
        std::cout << "  " << argv[0] << " genkey \"My Name\" \"My Org\" \"Earth\" ./key.bin\n";
        std::cout << "  " << argv[0] << " fingerprint ./key.bin 123456\n";
        std::cout << "  " << argv[0] << " build ./src ./output.tefpkg 0x12345678 \\\n";
        std::cout << "        -e \"*.tmp\" -e \"cache/*\" -c compress.json\n";
        std::cout << "  " << argv[0] << " build ./src ./output.tefpkg 0x12345678 -n\n";
        return;
    }

    if (std::string command = argv[1]; command == "list") {
        if (argc < 3) {
            std::cerr << "ERROR: Missing arguments for list\n";
            std::cerr << "Usage: " << argv[0] << " list <dir> [exclude...]\n";
            return;
        }

        std::filesystem::path dir = argv[2];
        std::vector<std::string_view> exclude;

        for (int i = 3; i < argc; ++i) {
            exclude.emplace_back(argv[i]);
        }

        std::cout << "Listing files in: " << dir << std::endl;
        std::cout << "Exclude patterns: " << exclude.size() << std::endl;

        print_directory_files(dir, exclude, 2);

    } else if (command == "tomacro") {
        if (argc < 4) {
            std::cerr << "ERROR: Missing arguments for tomacro\n";
            std::cerr << "Usage: " << argv[0] << " tomacro <dir> <output.h> [exclude...]\n";
            return;
        }

        std::filesystem::path dir = argv[2];
        std::filesystem::path output_file = argv[3];
        std::vector<std::string_view> exclude;

        for (int i = 4; i < argc; ++i) {
            exclude.emplace_back(argv[i]);
        }

        std::cout << "Converting directory to C macros: " << dir << std::endl;
        std::cout << "Output: " << output_file << std::endl;
        std::cout << "Exclude patterns: " << exclude.size() << std::endl;

        convert_directory_to_c_macro(dir, output_file, exclude, 2);

    } else if (command == "genkey") {
        if (argc < 6) {
            std::cerr << "ERROR: Missing arguments for genkey\n";
            std::cerr << "Usage: " << argv[0] << " genkey <author> <org> <loc> <output>\n";
            return;
        }

        siphash_config_t config = {
            .author = argv[2],
            .organization = argv[3],
            .location = argv[4]
        };

        std::filesystem::path output = argv[5];

        std::cout << "Generating SipHash key file\n";
        std::cout << "Author: " << config.author << std::endl;
        std::cout << "Organization: " << config.organization << std::endl;
        std::cout << "Location: " << config.location << std::endl;
        std::cout << "Output: " << output << std::endl;

        generate_key_file(config, output);

    } else if (command == "fingerprint") {
        if (argc < 3) {
            std::cerr << "ERROR: Missing arguments for fingerprint\n";
            std::cerr << "Usage: " << argv[0] << " fingerprint <key_file> [seed]\n";
            return;
        }

        std::filesystem::path key_file = argv[2];
        uint64_t seed = 0;

        if (argc >= 4) {
            if (std::string seed_str = argv[3]; seed_str.substr(0, 2) == "0x") {
                seed = std::stoull(seed_str.substr(2), nullptr, 16);
            } else {
                seed = std::stoull(seed_str);
            }
        }

        std::cout << "Generating fingerprint from key file: " << key_file << std::endl;
        std::cout << "Seed: 0x" << std::hex << seed << std::dec << std::endl;

        uint64_t fingerprint = generate_fingerprint(key_file, seed);

        std::cout << "Fingerprint: 0x" << std::hex << std::uppercase
                  << fingerprint << std::dec << std::endl;

    } else if (command == "build") {
        if (argc < 4) {
            std::cerr << "ERROR: Missing arguments for build\n";
            std::cerr << "Usage: " << argv[0] << " build <dir> <output> [fingerprint] [options]\n";
            std::cerr << "Options:\n";
            std::cerr << "  -e, --exclude <pattern>    Exclude pattern (can be multiple)\n";
            std::cerr << "  -c, --compress <json>      Compression config JSON file\n";
            std::cerr << "  -n, --no-file-list         Do not generate file list (default: generate)\n";
            return;
        }

        std::filesystem::path dir = argv[2];
        std::filesystem::path output = argv[3];
        uint64_t fingerprint = 0x114514;  // 默认指纹
        bool generate_file_list = true;   // 默认生成文件列表

        std::vector<std::string_view> exclude;
        std::unordered_map<std::string, CompressConfig> compress_map;
        std::filesystem::path compress_config_file;

        // 解析参数
        for (int i = 4; i < argc; ++i) {
            if (std::string arg = argv[i]; arg == "-e" || arg == "--exclude") {
                if (i + 1 < argc) {
                    exclude.emplace_back(argv[++i]);
                }
            } else if (arg == "-c" || arg == "--compress") {
                if (i + 1 < argc) {
                    compress_config_file = argv[++i];
                }
            } else if (arg == "-n" || arg == "--no-file-list") {
                generate_file_list = false;
            } else if (arg.substr(0, 2) == "0x") {
                // 指纹参数
                fingerprint = std::stoull(arg.substr(2), nullptr, 16);
            } else if (std::all_of(arg.begin(), arg.end(), ::isdigit)) {
                // 纯数字指纹
                fingerprint = std::stoull(arg);
            } else if (i == 4 && (arg[0] != '-')) {
                // 位置参数指纹
                if (arg.substr(0, 2) == "0x") {
                    fingerprint = std::stoull(arg.substr(2), nullptr, 16);
                } else {
                    fingerprint = std::stoull(arg);
                }
            }
        }

        // 加载压缩配置
        if (!compress_config_file.empty()) {
            compress_map = load_compress_config_from_json(compress_config_file);
        }

        std::cout << "Building package from: " << dir << std::endl;
        std::cout << "Output: " << output << std::endl;
        std::cout << "Fingerprint: 0x" << std::hex << fingerprint << std::dec << std::endl;
        std::cout << "Generate file list: " << (generate_file_list ? "yes" : "no") << std::endl;
        std::cout << "Exclude patterns: " << exclude.size() << std::endl;
        std::cout << "Compress configs: " << compress_map.size() << std::endl;

        // 调用修改后的build_tefpkg函数，传入生成文件列表的开关
        build_tefpkg(dir, output, fingerprint, generate_file_list, exclude, compress_map);

    } else {
        std::cerr << "ERROR: Unknown command: " << command << std::endl;
    }
}

int main(const int argc, char* argv[]) {
    try {
        handle_command_line(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}