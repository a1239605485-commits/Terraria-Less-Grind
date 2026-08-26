# 📦 TEFPkg-Tool

TEFPkg-Tool is the official TEF package format packaging tool, used to package components such as Plugins, Modules, and ModLoaders into `.tefpkg` files for loading and use in TEFKernel.

---

## 📑 Table of Contents

<details>
<summary><b>📖 Click to expand full table of contents</b></summary>

- [📖 Overview](#-overview)
  - [What is TEFPkg-Tool](#what-is-tefpkg-tool)
  - [Core Features](#core-features)
  - [Supported File Types](#supported-file-types)
- [🚀 Quick Start](#-quick-start)
  - [Installation and Compilation](#installation-and-compilation)
  - [Basic Usage](#basic-usage)
- [📚 Command Reference](#-command-reference)
  - [list - List Directory Files](#list---list-directory-files)
  - [tomacro - Generate C Macros](#tomacro---generate-c-macros)
  - [genkey - Generate Key File](#genkey---generate-key-file)
  - [fingerprint - Generate Fingerprint](#fingerprint---generate-fingerprint)
  - [build - Build TEF Package](#build---build-tef-package)
- [📝 Configuration File](#-configuration-file)
  - [Compression Configuration JSON](#compression-configuration-json)
  - [File Exclusion Rules](#file-exclusion-rules)
- [💡 Complete Examples](#-complete-examples)
  - [Packaging a Plugin](#packaging-a-plugin)
  - [Packaging a Module](#packaging-a-module)
  - [Packaging a ModLoader](#packaging-a-modloader)
- [📂 Output Structure](#-output-structure)
- [⚠️ Important Notes](#-important-notes)
- [🔗 Related Links](#-related-links)

</details>

---

## 📖 Overview

### What is TEFPkg-Tool

TEFPkg-Tool is a command-line tool that packages files from a directory into TEF package (`.tefpkg`) format. It is designed specifically for TEFKernel and can automatically identify package types (Plugin/Module/ModLoader) and organize files according to TEFKernel specifications.

### Core Features

| Feature                       | Description                                                 |
|:------------------------------|:------------------------------------------------------------|
| **🔍 Auto Type Detection**    | Automatically identifies Plugin/Module/ModLoader types      |
| **📋 File List Generation**   | Automatically generates file index with ID-based references |
| **🗜️ Flexible Compression**   | Supports LZ4/LZ4HC compression with per-file configuration  |
| **🔐 Package Signing**        | Supports SipHash signature verification                     |
| **🚫 File Exclusion**         | Supports wildcard exclusion for files to omit               |
| **🏗️ Multi-Platform Support** | Automatically handles multi-platform library naming         |

### Supported File Types

The tool automatically recognizes the following package types:

| Package Type | Detection Rule                 | Purpose           |
|:-------------|:-------------------------------|:------------------|
| **Plugin**   | `libplugin.*.so` exists in dir | Plugin package    |
| **Module**   | `libmodule.*.so` exists in dir | Module package    |
| **Loader**   | `libloader.*.so` exists in dir | ModLoader package |

---

## 🚀 Quick Start

### Installation and Compilation

```bash
# Clone the repository
git clone https://github.com/eternalfuture-e38299/TEFPkg-Tool.git
cd TEFPkg-Tool

# Compile
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make

# Install to system path (optional)
sudo make install
```

### Basic Usage

```bash
# View help
./tefpkg_tool

# Basic packaging command
./tefpkg_tool build ./source_dir ./output.tefpkg 0x12345678
```

---

## 📚 Command Reference

### list - List Directory Files

Lists all files in the specified directory with their assigned IDs.

```bash
./tefpkg_tool list <dir> [exclude...]
```

**Parameter Description:**

| Parameter      | Description                                      |
|:---------------|:-------------------------------------------------|
| `<dir>`        | Directory path to list                           |
| `[exclude...]` | Optional exclusion patterns (supports wildcards) |

**Example:**

```bash
# List all files in src directory
./tefpkg_tool list ./src

# Exclude temporary files and cache directories
./tefpkg_tool list ./src "*.tmp" "cache/*"
```

**Output Example:**

```
File list for directory: ./src
Total files: 15
------------------------------------------------------------
ID    Path
------------------------------------------------------------
2     libplugin.android.arm64.so
3     libplugin.android.arm.so
4     libplugin.linux.x64.so
...
```

---

### tomacro - Generate C Macros

Converts directory file list to C language macro definition header file for referencing file IDs in code.

```bash
./tefpkg_tool tomacro <dir> <output.h> [exclude...]
```

**Parameter Description:**

| Parameter      | Description                       |
|:---------------|:----------------------------------|
| `<dir>`        | Source directory path             |
| `<output.h>`   | Output header file path           |
| `[exclude...]` | Optional exclusion patterns       |

**Example:**

```bash
# Generate file ID macro definitions
./tefpkg_tool tomacro ./src ./file_ids.h "*.tmp"
```

**Output Example (`file_ids.h`):**

```c
/**
 * Auto-generated file ID macros
 * Generated from directory: ./src
 * DO NOT EDIT THIS FILE DIRECTLY
 */

#pragma once

#define FILE_ID_LIBPLUGIN_ANDROID_ARM64_SO     2  // libplugin.android.arm64.so
#define FILE_ID_LIBPLUGIN_ANDROID_ARM_SO       3  // libplugin.android.arm.so
#define FILE_ID_LIBPLUGIN_LINUX_X64_SO         4  // libplugin.linux.x64.so

// Total files: 15
```

---

### genkey - Generate Key File

Generates a SipHash key file for package signing.

```bash
./tefpkg_tool genkey <author> <org> <loc> <output>
```

**Parameter Description:**

| Parameter  | Description          |
|:-----------|:---------------------|
| `<author>` | Author name          |
| `<org>`    | Organization name    |
| `<loc>`    | Location information |
| `<output>` | Output key file path |

**Example:**

```bash
# Generate key file
./tefpkg_tool genkey "TEFKernel Team" "TEFKernel" "Earth" ./key.bin
```

---

### fingerprint - Generate Fingerprint

Generates a fingerprint from the key file for package signature verification.

```bash
./tefpkg_tool fingerprint <key_file> [seed]
```

**Parameter Description:**

| Parameter    | Description                      |
|:-------------|:---------------------------------|
| `<key_file>` | Key file path                    |
| `[seed]`     | Seed value (optional, default 0) |

**Example:**

```bash
# Generate fingerprint (using default seed)
./tefpkg_tool fingerprint ./key.bin

# Using custom seed
./tefpkg_tool fingerprint ./key.bin 0x12345678
```

**Output Example:**

```
Generating fingerprint from key file: ./key.bin
Seed: 0x0
Fingerprint: 0xABCDEF1234567890
```

---

### build - Build TEF Package

Packages a directory into a `.tefpkg` file. This is the most commonly used command.

```bash
./tefpkg_tool build <dir> <output> [fingerprint] [options]
```

**Parameter Description:**

| Parameter       | Description                            |
|:----------------|:---------------------------------------|
| `<dir>`         | Source directory path                  |
| `<output>`      | Output `.tefpkg` file path             |
| `[fingerprint]` | Fingerprint value (default `0x114514`) |

**Options:**

| Option                    | Description                                         |
|:--------------------------|:----------------------------------------------------|
| `-e, --exclude <pattern>` | Exclude matching files (can be used multiple times) |
| `-c, --compress <json>`   | Compression configuration file path                 |
| `-n, --no-file-list`      | Do not generate file list (generated by default)    |

**Examples:**

```bash
# Basic packaging
./tefpkg_tool build ./src ./output.tefpkg 0x12345678

# With exclusion rules
./tefpkg_tool build ./src ./output.tefpkg 0x12345678 -e "*.tmp" -e "cache/*"

# Using compression configuration
./tefpkg_tool build ./src ./output.tefpkg 0x12345678 -c compress.json

# No file list generation (reduces package size)
./tefpkg_tool build ./src ./output.tefpkg 0x12345678 -n
```

---

## 📝 Configuration File

### Compression Configuration JSON

Through a JSON configuration file, different compression strategies can be specified for different files.

**File Format:**

```json
{
    "*.png": {
        "mode": "lz4hc",
        "level": 9
    },
    "*.mp3": {
        "mode": "none"
    },
    "libplugin.*.so": {
        "mode": "lz4",
        "level": 1
    },
    "assets/": {
        "mode": "lz4hc",
        "level": 9
    }
}
```

**Configuration Fields:**

| Field   | Type   | Description                                       |
|:--------|:-------|:--------------------------------------------------|
| `mode`  | string | Compression mode: `none`, `lz4`, `lz4hc`          |
| `level` | int    | Compression level: 1-9 (only effective for lz4hc) |

**Matching Rules (priority from highest to lowest):**

1. **Exact Match**: Full path exact match
2. **Wildcard Match**: `*` and `?` wildcards
3. **Directory Prefix Match**: Directory pattern ending with `/`
4. **Extension Match**: `.png`, `.mp3`, etc.
5. **Default**: `lz4` level 1

**Example:**

```bash
# Package using compression configuration file
./tefpkg_tool build ./src ./output.tefpkg 0x12345678 -c compress.json
```

### File Exclusion Rules

The `-e` option supports wildcard exclusion:

| Pattern   | Description                             |
|:----------|:----------------------------------------|
| `*.tmp`   | Exclude all `.tmp` files                |
| `cache/*` | Exclude all files in `cache/` directory |
| `*.log`   | Exclude all `.log` files                |

**Example:**

```bash
# Exclude multiple file types
./tefpkg_tool build ./src ./output.tefpkg 0x12345678 \
    -e "*.tmp" \
    -e "*.log" \
    -e "cache/*" \
    -e ".*"  # Exclude hidden files
```

---

## 💡 Complete Examples

### Packaging a Plugin

**Directory Structure:**

```
my_plugin/
├── libplugin.android.arm64.so
├── libplugin.android.arm.so
├── libplugin.linux.x64.so
├── libplugin.windows.x64.dll
├── config.json
└── icon.png
```

**Packaging Commands:**

```bash
# 1. Generate key (optional)
./tefpkg_tool genkey "MyTeam" "MyOrg" "World" ./key.bin

# 2. Generate fingerprint
FINGERPRINT=$(./tefpkg_tool fingerprint ./key.bin 0x114514 | grep -o '0x[A-F0-9]*')

# 3. Create compression configuration
cat > compress.json << EOF
{
    "*.png": {"mode": "lz4hc", "level": 9},
    "*.json": {"mode": "lz4", "level": 1},
    "libplugin.*.so": {"mode": "lz4", "level": 1}
}
EOF

# 4. Package
./tefpkg_tool build ./my_plugin ./my_plugin.tefpkg ${FINGERPRINT} -c compress.json
```

### Packaging a Module

**Directory Structure:**

```
my_module/
├── libmodule.android.arm64.so
├── libmodule.android.arm.so
├── libmodule.linux.x64.so
├── textures/
│   ├── grass.png
│   └── stone.png
└── sounds/
    └── ambient.mp3
```

**Packaging Commands:**

```bash
# Compression configuration (different strategies for audio and images)
cat > compress.json << EOF
{
    "textures/*.png": {"mode": "lz4hc", "level": 9},
    "sounds/*.mp3": {"mode": "none"},
    "*.so": {"mode": "lz4", "level": 1}
}
EOF

# Package
./tefpkg_tool build ./my_module ./my_module.tefpkg 0x12345678 -c compress.json
```

### Packaging a ModLoader

**Directory Structure:**

```
my_modloader/
├── libloader.android.arm64.so
├── libloader.android.arm.so
├── libloader.linux.x64.so
├── libloader.windows.x64.dll
├── default_config.json
└── builtin_mods/
    ├── mod1.tefpkg
    └── mod2.tefpkg
```

**Packaging Commands:**

```bash
# Exclude built-in Mods (they will be packaged separately)
./tefpkg_tool build ./my_modloader ./my_modloader.tefpkg 0x12345678 \
    -e "builtin_mods/*"
```

---

## 📂 Output Structure

The packaged `.tefpkg` file contains the following content:

```
my_package.tefpkg
├── [Header]
│   ├── magic: "TEFP"
│   ├── version: 0x0200
│   ├── file_count: N
│   └── signature: 0x...
├── [Entry 0] File list (ID: 0)
├── [Entry 1-11] Fixed library files (depending on type)
│   ├── Entry 1: libplugin.android.arm64.so (or empty placeholder)
│   ├── Entry 2: libplugin.android.arm.so (or empty placeholder)
│   └── ...
└── [Entry 12-N] User files
    ├── config.json
    ├── icon.png
    └── ...
```

---

## ⚠️ Important Notes

### File Naming Conventions

| Package Type | Naming Format                     | Example                      |
|:-------------|:----------------------------------|:-----------------------------|
| **Plugin**   | `libplugin.{system}.{arch}.{ext}` | `libplugin.android.arm64.so` |
| **Module**   | `libmodule.{system}.{arch}.{ext}` | `libmodule.linux.x64.so`     |
| **Loader**   | `libloader.{system}.{arch}.{ext}` | `loader.windows.x64.dll`     |

### Supported Platform Architectures

| System    | Architecture | Extension |
|:----------|:-------------|:----------|
| Android   | arm64, arm   | .so       |
| Linux     | x64, x86     | .so       |
| Windows   | x64, x86     | .dll      |
| macOS     | arm64, x64   | .dylib    |

### File ID Allocation

| ID Range | Purpose                                      |
|:---------|:---------------------------------------------|
| 0        | File list (automatically assigned)           |
| 1-11     | Fixed library files (automatically assigned) |
| 12+      | User files (alphabetically ordered)          |

### Frequently Asked Questions

**Q: How do I specify the package type?**

A: The tool auto-detects it. Ensure the directory contains library files with the corresponding naming conventions.

**Q: What if there are no library files?**

A: The tool will detect no library files and process it as a generic package, with all file IDs starting from 1.

**Q: How do I reduce package size?**

A: Use the `-n` option to skip file list generation and configure reasonable compression strategies.

**Q: How do I verify after packaging?**

A: The tool automatically executes `tefpkg_verify_pkg` and `tefpkg_verify_signature` verification.

---

## 🔗 Related Links

- [TEFKernel GitHub](https://github.com/eternalfuture-e38299/tefkernel)

---

*Happy Packaging! 📦🚀✨*