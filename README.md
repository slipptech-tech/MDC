
<div align="center">

  **MDC**

  *Modern Windows Disk Analyzer*

  ![Platform](https://img.shields.io/badge/platform-Windows-0078D4?style=flat-square)
  ![Language](https://img.shields.io/badge/language-C%2B%2B-00599C?style=flat-square)
  ![Build](https://img.shields.io/badge/build-MSVC-6C5CE7?style=flat-square)
  ![License](https://img.shields.io/badge/license-MIT-green?style=flat-square)

  **Analyze. Explore. Manage.**

</div>

---

## Overview

MDC is a lightweight Windows disk analyzer built with C++ and the Win32 API.

It helps users explore folders, analyze disk usage, identify large files, and manage selected files through a clean desktop interface.

## Features

- **Folder Scanning** — scan selected directories and drives.
- **Disk Analysis** — inspect file sizes and storage usage.
- **File Browser** — view discovered files and their locations.
- **Search** — quickly find files by name.
- **Category Filtering** — organize results by file type.
- **File Explorer Integration** — open a file's location in Windows Explorer.
- **File Management** — move selected files to the Recycle Bin.
- **CSV Export** — export scan results for further analysis.
- **Dark Interface** — modern dark-themed desktop UI.
- **Animated Progress** — visual feedback during scanning.

## Requirements

- Windows 10 or later
- Visual Studio 2022 / 2026
- MSVC C++ compiler
- Windows SDK
- C++17 compatible compiler

## Build

Clone the repository:

```bash
git clone https://github.com/slipptech-tech/MDC.git
cd MDC
```

Build using the provided script:

```bat
build.bat
```

The executable will be generated in the build output directory.

## Usage

1. Launch `MDC.exe`.
2. Select a folder or drive.
3. Start scanning.
4. Browse the discovered files.
5. Use search and category filters.
6. Open file locations or export results.

**Note:** File deletion requires confirmation. Selected files are sent to the Windows Recycle Bin rather than being permanently deleted.

## Project Structure

```text
MDC/
├── main.cpp
├── build.bat
├── README.md
└── mdc/
    ├── theme.h
    └── animation.h
```

## Technology

| Component | Technology |
|---|---|
| Language | C++ |
| Interface | Win32 API |
| Graphics | GDI+ |
| File System | C++17 filesystem |
| Platform | Windows |
| Compiler | MSVC |

## License

This project is licensed under the MIT License.

See [LICENSE](LICENSE) for details.

---

<div align="center">

  **MDC**

  *Developed by Slipp Tech*

</div>
