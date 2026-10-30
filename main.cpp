
#define NOMINMAX
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <gdiplus.h>

#include <filesystem>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <memory>
#include <cwctype>
#include <cstdio>
#include <cmath>

#include "mdc/theme.h"
#include "mdc/animation.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ole32.lib")

using namespace Gdiplus;
namespace fs = std::filesystem;

constexpr UINT WM_SCAN_PROGRESS = WM_APP + 1;
constexpr UINT WM_SCAN_FINISHED = WM_APP + 2;
constexpr int SIDEBAR_WIDTH = 226;
constexpr int WINDOW_MIN_WIDTH = 1050;
constexpr int WINDOW_MIN_HEIGHT = 700;

struct UiRect {
    int x, y, w, h;

    bool Contains(int px, int py) const {
        return px >= x && px <= x + w &&
               py >= y && py <= y + h;
    }
};

UiRect R(int x, int y, int w, int h) {
    return { x, y, w, h };
}

struct FileEntry {
    std::wstring name;
    std::wstring path;
    std::wstring category;
    std::wstring assessment;
    unsigned long long size = 0;
};

enum Page {
    PAGE_OVERVIEW,
    PAGE_CLEANUP,
    PAGE_REVIEW,
    PAGE_PROTECTED
};

HWND g_hwnd = nullptr;
ULONG_PTR g_gdiplusToken = 0;

std::vector<FileEntry> g_files;
std::vector<FileEntry> g_visibleFiles;

std::mutex g_filesMutex;

std::atomic<bool> g_scanning(false);
std::atomic<bool> g_cancelScan(false);
std::atomic<int> g_progress(0);
std::atomic<unsigned long long> g_scannedBytes(0);

std::thread g_worker;

Page g_page = PAGE_OVERVIEW;

int g_width = 1280;
int g_height = 820;
int g_scroll = 0;
int g_selected = -1;
int g_hoverButton = -1;

std::wstring g_search;
std::wstring g_status = L"Ready. Select a folder to scan.";
std::wstring g_drive = L"C:\\";

bool g_searchFocused = false;
bool g_scanFinished = false;

float g_animation = 0.0f;

Font* g_font = nullptr;
Font* g_smallFont = nullptr;
Font* g_titleFont = nullptr;

std::wstring Lower(std::wstring value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](wchar_t c) {
            return (wchar_t)towlower(c);
        }
    );

    return value;
}

void DrawRoundRect(
    Graphics& graphics,
    UiRect rect,
    int radius,
    Color color
) {
    GraphicsPath path;

    REAL x = (REAL)rect.x;
    REAL y = (REAL)rect.y;
    REAL w = (REAL)rect.w;
    REAL h = (REAL)rect.h;
    REAL d = (REAL)(radius * 2);

    if (w <= 0 || h <= 0)
        return;

    d = std::min(d, std::min(w, h));

    path.AddArc(x, y, d, d, 180.0f, 90.0f);
    path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();

    SolidBrush brush(color);
    graphics.FillPath(&brush, &path);
}

void DrawBorder(
    Graphics& graphics,
    UiRect rect,
    int radius,
    Color color
) {
    GraphicsPath path;

    REAL x = (REAL)rect.x;
    REAL y = (REAL)rect.y;
    REAL w = (REAL)rect.w;
    REAL h = (REAL)rect.h;
    REAL d = (REAL)(radius * 2);

    if (w <= 0 || h <= 0)
        return;

    d = std::min(d, std::min(w, h));

    path.AddArc(x, y, d, d, 180.0f, 90.0f);
    path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();

    Pen pen(color, 1.0f);
    graphics.DrawPath(&pen, &path);
}

void DrawLabel(
    Graphics& graphics,
    const std::wstring& text,
    int x,
    int y,
    Color color,
    Font* font = nullptr
) {
    SolidBrush brush(color);

    graphics.DrawString(
        text.c_str(),
        -1,
        font ? font : g_font,
        PointF((REAL)x, (REAL)y),
        &brush
    );
}

void DrawCenteredLabel(
    Graphics& graphics,
    const std::wstring& text,
    UiRect rect,
    Color color
) {
    StringFormat format;
    format.SetAlignment(StringAlignmentCenter);
    format.SetLineAlignment(StringAlignmentCenter);

    SolidBrush brush(color);

    graphics.DrawString(
        text.c_str(),
        -1,
        g_font,
        RectF(
            (REAL)rect.x,
            (REAL)rect.y,
            (REAL)rect.w,
            (REAL)rect.h
        ),
        &format,
        &brush
    );
}

void DrawButton(
    Graphics& graphics,
    UiRect rect,
    const std::wstring& text,
    bool primary,
    bool hovered
) {
    Color background = primary
        ? (hovered ? MDCTheme::AccentHover : MDCTheme::Accent)
        : (hovered ? MDCTheme::CardHover : MDCTheme::Card);

    DrawRoundRect(graphics, rect, 9, background);

    if (!primary)
        DrawBorder(graphics, rect, 9, MDCTheme::Border);

    DrawCenteredLabel(graphics, text, rect, MDCTheme::Text);
}

std::wstring FormatSize(unsigned long long bytes) {
    const wchar_t* units[] = {
        L"B", L"KB", L"MB", L"GB", L"TB"
    };

    double size = (double)bytes;
    int unit = 0;

    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        ++unit;
    }

    std::wstringstream stream;

    stream << std::fixed
           << std::setprecision(unit == 0 ? 0 : 2)
           << size << L" " << units[unit];

    return stream.str();
}

std::wstring GetCategory(const fs::path& path) {
    std::wstring ext = Lower(path.extension().wstring());

    if (ext == L".tmp" || ext == L".temp" ||
        ext == L".log" || ext == L".cache")
        return L"Temporary / Log";

    if (ext == L".bak" || ext == L".old")
        return L"Backup";

    if (ext == L".zip" || ext == L".rar" ||
        ext == L".7z")
        return L"Archive";

    if (ext == L".mp4" || ext == L".mkv" ||
        ext == L".avi" || ext == L".mov")
        return L"Video";

    if (ext == L".jpg" || ext == L".jpeg" ||
        ext == L".png" || ext == L".webp")
        return L"Image";

    if (ext == L".exe" || ext == L".dll" ||
        ext == L".sys")
        return L"Application";

    if (ext == L".doc" || ext == L".docx" ||
        ext == L".pdf" || ext == L".txt")
        return L"Document";

    return L"Other";
}

std::wstring GetAssessment(const std::wstring& category) {
    if (category == L"Temporary / Log" ||
        category == L"Backup")
        return L"Review";

    if (category == L"Application")
        return L"Protected";

    return L"Information";
}

void RefreshVisibleFiles() {
    g_visibleFiles.clear();

    std::wstring query = Lower(g_search);

    std::lock_guard<std::mutex> lock(g_filesMutex);

    for (const auto& file : g_files) {
        if (!query.empty()) {
            if (Lower(file.name).find(query) == std::wstring::npos &&
                Lower(file.path).find(query) == std::wstring::npos)
                continue;
        }

        if (g_page == PAGE_CLEANUP &&
            file.assessment != L"Review")
            continue;

        if (g_page == PAGE_PROTECTED &&
            file.assessment != L"Protected")
            continue;

        g_visibleFiles.push_back(file);
    }

    g_selected = -1;
    g_scroll = 0;
}

size_t GetFileCount() {
    std::lock_guard<std::mutex> lock(g_filesMutex);
    return g_files.size();
}

unsigned long long GetTotalSize() {
    std::lock_guard<std::mutex> lock(g_filesMutex);

    unsigned long long total = 0;

    for (const auto& file : g_files)
        total += file.size;

    return total;
}

void ScanWorker() {
    std::vector<FileEntry> results;

    unsigned long long scannedBytes = 0;
    size_t scannedFiles = 0;

    try {
        std::error_code ec;

        fs::recursive_directory_iterator iterator(
            fs::path(g_drive),
            fs::directory_options::skip_permission_denied,
            ec
        );

        fs::recursive_directory_iterator end;

        while (iterator != end && !g_cancelScan) {
            ec.clear();

            const fs::directory_entry& current = *iterator;

            if (current.is_symlink(ec)) {
                if (current.is_directory(ec))
                    iterator.disable_recursion_pending();

                iterator.increment(ec);
                continue;
            }

            if (current.is_regular_file(ec)) {
                auto size = current.file_size(ec);

                if (!ec) {
                    FileEntry entry;

                    entry.path = current.path().wstring();
                    entry.name = current.path().filename().wstring();
                    entry.size = size;
                    entry.category = GetCategory(current.path());
                    entry.assessment = GetAssessment(entry.category);

                    results.push_back(std::move(entry));

                    scannedBytes += size;
                    ++scannedFiles;
                }
            }

            if (scannedFiles % 100 == 0) {
                g_scannedBytes = scannedBytes;
                g_progress = (int)std::min<size_t>(
                    95,
                    scannedFiles / 100
                );

                PostMessageW(
                    g_hwnd,
                    WM_SCAN_PROGRESS,
                    (WPARAM)scannedFiles,
                    0
                );
            }

            iterator.increment(ec);
        }
    }
    catch (...) {
        // Filesystem errors are skipped.
    }

    if (g_cancelScan)
        return;

    auto* output = new std::vector<FileEntry>(
        std::move(results)
    );

    if (!PostMessageW(
        g_hwnd,
        WM_SCAN_FINISHED,
        0,
        (LPARAM)output
    )) {
        delete output;
    }
}

void StartScan() {
    if (g_scanning)
        return;

    if (g_worker.joinable())
        g_worker.join();

    {
        std::lock_guard<std::mutex> lock(g_filesMutex);
        g_files.clear();
    }

    g_visibleFiles.clear();
    g_selected = -1;
    g_scroll = 0;

    g_progress = 0;
    g_scannedBytes = 0;

    g_scanFinished = false;
    g_cancelScan = false;
    g_scanning = true;

    g_status = L"Scanning selected folder...";

    InvalidateRect(g_hwnd, nullptr, FALSE);

    g_worker = std::thread(ScanWorker);
}

bool ChooseFolder() {
    BROWSEINFOW info{};

    info.hwndOwner = g_hwnd;
    info.lpszTitle = L"Select a folder to scan";
    info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&info);

    if (!pidl)
        return false;

    wchar_t selectedPath[MAX_PATH]{};

    bool success = SHGetPathFromIDListW(
        pidl,
        selectedPath
    );

    CoTaskMemFree(pidl);

    if (!success)
        return false;

    g_drive = selectedPath;

    g_status = L"Folder selected. Ready to scan.";
    InvalidateRect(g_hwnd, nullptr, FALSE);

    return true;
}

void OpenFileLocation() {
    if (g_selected < 0 ||
        g_selected >= (int)g_visibleFiles.size()) {
        MessageBoxW(
            g_hwnd,
            L"Select a file first.",
            L"MDC",
            MB_OK | MB_ICONINFORMATION
        );
        return;
    }

    const std::wstring path = g_visibleFiles[g_selected].path;

    std::wstring parameters = L"/select,\"" + path + L"\"";

    HINSTANCE result = ShellExecuteW(
        g_hwnd,
        L"open",
        L"explorer.exe",
        parameters.c_str(),
        nullptr,
        SW_SHOWNORMAL
    );

    if ((INT_PTR)result <= 32) {
        MessageBoxW(
            g_hwnd,
            L"Could not open File Explorer.",
            L"MDC",
            MB_OK | MB_ICONERROR
        );
    }
}

void DeleteSelected() {
    if (g_selected < 0 ||
        g_selected >= (int)g_visibleFiles.size()) {
        MessageBoxW(
            g_hwnd,
            L"Select a file first.",
            L"MDC",
            MB_OK | MB_ICONINFORMATION
        );
        return;
    }

    const FileEntry entry = g_visibleFiles[g_selected];

    std::wstring message =
        L"Move this file to the Windows Recycle Bin?\n\n" +
        entry.path;

    if (MessageBoxW(
        g_hwnd,
        message.c_str(),
        L"Confirm deletion",
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2
    ) != IDYES)
        return;

    std::wstring path = entry.path;
    path.push_back(L'\0');
    path.push_back(L'\0');

    SHFILEOPSTRUCTW operation{};

    operation.hwnd = g_hwnd;
    operation.wFunc = FO_DELETE;
    operation.pFrom = path.c_str();
    operation.fFlags =
        FOF_ALLOWUNDO |
        FOF_NOCONFIRMATION |
        FOF_SILENT |
        FOF_NOERRORUI;

    int result = SHFileOperationW(&operation);

    if (result == 0 && !operation.fAnyOperationsAborted) {
        {
            std::lock_guard<std::mutex> lock(g_filesMutex);

            g_files.erase(
                std::remove_if(
                    g_files.begin(),
                    g_files.end(),
                    [&](const FileEntry& file) {
                        return file.path == entry.path;
                    }
                ),
                g_files.end()
            );
        }

        RefreshVisibleFiles();
        g_status = L"File moved to Recycle Bin.";
    }
    else {
        g_status = L"Could not move file to Recycle Bin.";

        MessageBoxW(
            g_hwnd,
            L"Windows could not move this file to the Recycle Bin.",
            L"MDC",
            MB_OK | MB_ICONERROR
        );
    }

    InvalidateRect(g_hwnd, nullptr, FALSE);
}

void ExportCSV() {
    wchar_t filename[MAX_PATH] = L"MDC-report.csv";

    OPENFILENAMEW dialog{};

    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = g_hwnd;
    dialog.lpstrFilter =
        L"CSV files (*.csv)\0*.csv\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = filename;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    dialog.lpstrDefExt = L"csv";

    if (!GetSaveFileNameW(&dialog))
        return;

    FILE* file = nullptr;

    if (_wfopen_s(&file, filename, L"wb") != 0 || !file) {
        MessageBoxW(
            g_hwnd,
            L"Could not create report.",
            L"MDC",
            MB_OK | MB_ICONERROR
        );
        return;
    }

    fwprintf(
        file,
        L"File Name,Size,Category,Assessment,Full Path\n"
    );

    auto escape = [](std::wstring value) {
        size_t position = 0;

        while ((position = value.find(L'"', position))
            != std::wstring::npos) {
            value.insert(position, L"\"");
            position += 2;
        }

        return L"\"" + value + L"\"";
    };

    {
        std::lock_guard<std::mutex> lock(g_filesMutex);

        for (const auto& entry : g_files) {
            fwprintf(
                file,
                L"%ls,%llu,%ls,%ls,%ls\n",
                escape(entry.name).c_str(),
                entry.size,
                escape(entry.category).c_str(),
                escape(entry.assessment).c_str(),
                escape(entry.path).c_str()
            );
        }
    }

    fclose(file);

    g_status = L"CSV report exported successfully.";
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

void DrawSidebar(Graphics& graphics) {
    using namespace MDCTheme;

    DrawRoundRect(
        graphics,
        R(0, 0, SIDEBAR_WIDTH, g_height),
        0,
        Sidebar
    );

    DrawLabel(graphics, L"MDC", 28, 25, Text, g_titleFont);

    DrawLabel(
        graphics,
        L"STORAGE INTELLIGENCE",
        29, 61,
        Secondary,
        g_smallFont
    );

    DrawLabel(
        graphics,
        L"WORKSPACE",
        29, 117,
        Muted,
        g_smallFont
    );

    const wchar_t* labels[] = {
        L"Overview",
        L"Cleanup candidates",
        L"Manual review",
        L"Protected files"
    };

    for (int i = 0; i < 4; ++i) {
        UiRect rect = R(16, 145 + i * 53, 194, 43);

        bool active = ((int)g_page == i);
        bool hovered = (g_hoverButton == i);

        if (active)
            DrawRoundRect(graphics, rect, 8, AccentSoft);
        else if (hovered)
            DrawRoundRect(graphics, rect, 8, CardHover);

        if (active) {
            DrawRoundRect(
                graphics,
                R(16, rect.y + 8, 3, 27),
                2,
                Accent
            );
        }

        DrawLabel(
            graphics,
            labels[i],
            34,
            rect.y + 12,
            active ? Text : Secondary
        );
    }

    DrawLabel(
        graphics,
        L"TOOLS",
        29, 383,
        Muted,
        g_smallFont
    );

    DrawButton(
        graphics,
        R(16, 410, 194, 43),
        L"Export report",
        false,
        g_hoverButton == 4
    );

    DrawButton(
        graphics,
        R(16, 463, 194, 43),
        L"Delete selected",
        false,
        g_hoverButton == 5
    );

    DrawButton(
        graphics,
        R(16, 516, 194, 43),
        L"Open file location",
        false,
        g_hoverButton == 8
    );

    DrawLabel(
        graphics,
        L"VERSION 1.0.0",
        29,
        g_height - 36,
        Muted,
        g_smallFont
    );
}

void DrawStatCard(
    Graphics& graphics,
    UiRect rect,
    const std::wstring& title,
    const std::wstring& value,
    const std::wstring& description,
    Color accent
) {
    using namespace MDCTheme;

    DrawRoundRect(graphics, rect, 12, Card);
    DrawBorder(graphics, rect, 12, Border);

    DrawRoundRect(
        graphics,
        R(rect.x + 17, rect.y + 18, 3, 34),
        2,
        accent
    );

    DrawLabel(
        graphics,
        title,
        rect.x + 31,
        rect.y + 15,
        Secondary,
        g_smallFont
    );

    DrawLabel(
        graphics,
        value,
        rect.x + 31,
        rect.y + 36,
        Text,
        g_titleFont
    );

    DrawLabel(
        graphics,
        description,
        rect.x + 31,
        rect.y + 77,
        Muted,
        g_smallFont
    );
}

void DrawProgress(Graphics& graphics, UiRect rect) {
    using namespace MDCTheme;

    DrawRoundRect(graphics, rect, 5, Panel);

    if (g_scanning) {
        int width = 45;

        int offset = (int)(
            (sin(g_animation) + 1.0f) * 0.5f *
            (rect.w - width - 4)
        );

        DrawRoundRect(
            graphics,
            R(rect.x + 2 + offset, rect.y + 2, width, rect.h - 4),
            4,
            Accent
        );
    }
    else if (g_progress > 0) {
        int width = (int)(
            (rect.w - 4) * (g_progress.load() / 100.0)
        );

        DrawRoundRect(
            graphics,
            R(rect.x + 2, rect.y + 2, width, rect.h - 4),
            4,
            Accent
        );
    }
}

void DrawTable(Graphics& graphics) {
    using namespace MDCTheme;

    int x = SIDEBAR_WIDTH + 24;
    int y = 340;
    int width = g_width - x - 28;

    DrawRoundRect(
        graphics,
        R(x, y, width, 42),
        8,
        Panel
    );

    int nameX = x + 17;
    int sizeX = x + 350;
    int categoryX = x + 460;
    int assessmentX = x + 625;
    int pathX = x + 790;

    DrawLabel(graphics, L"FILE", nameX, y + 13, Secondary, g_smallFont);
    DrawLabel(graphics, L"SIZE", sizeX, y + 13, Secondary, g_smallFont);
    DrawLabel(graphics, L"CATEGORY", categoryX, y + 13, Secondary, g_smallFont);
    DrawLabel(graphics, L"ASSESSMENT", assessmentX, y + 13, Secondary, g_smallFont);
    DrawLabel(graphics, L"LOCATION", pathX, y + 13, Secondary, g_smallFont);

    int listY = y + 49;
    int listHeight = g_height - listY - 85;
    int rowHeight = 43;

    GraphicsState state = graphics.Save();

    graphics.SetClip(
        RectF(
            (REAL)x,
            (REAL)listY,
            (REAL)width,
            (REAL)listHeight
        )
    );

    for (int i = 0; i < (int)g_visibleFiles.size(); ++i) {
        int rowY = listY + i * rowHeight - g_scroll;

        if (rowY + rowHeight < listY ||
            rowY > listY + listHeight)
            continue;

        UiRect row = R(x, rowY, width, rowHeight);

        Color background = i == g_selected
            ? Selected
            : (i % 2 == 0 ? Row : RowAlternate);

        DrawRoundRect(graphics, row, 5, background);

        DrawLabel(
            graphics,
            g_visibleFiles[i].name,
            nameX,
            rowY + 12,
            Text
        );

        DrawLabel(
            graphics,
            FormatSize(g_visibleFiles[i].size),
            sizeX,
            rowY + 12,
            Secondary
        );

        DrawLabel(
            graphics,
            g_visibleFiles[i].category,
            categoryX,
            rowY + 12,
            Secondary
        );

        Color assessmentColor = Green;

        if (g_visibleFiles[i].assessment == L"Review")
            assessmentColor = Orange;
        else if (g_visibleFiles[i].assessment == L"Protected")
            assessmentColor = Red;

        DrawLabel(
            graphics,
            g_visibleFiles[i].assessment,
            assessmentX,
            rowY + 12,
            assessmentColor
        );

        std::wstring path = g_visibleFiles[i].path;

        if (path.length() > 44)
            path = L"..." + path.substr(path.length() - 41);

        DrawLabel(
            graphics,
            path,
            pathX,
            rowY + 13,
            Muted,
            g_smallFont
        );
    }

    graphics.Restore(state);

    if (g_visibleFiles.empty()) {
        DrawLabel(
            graphics,
            g_scanning
                ? L"Scanning files..."
                : L"No files to display",
            x + 20,
            listY + 24,
            Secondary
        );
    }
}

void PaintInterface(HDC hdc) {
    RECT client{};
    GetClientRect(g_hwnd, &client);

    g_width = client.right;
    g_height = client.bottom;

    Bitmap buffer(g_width, g_height);
    Graphics graphics(&buffer);

    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    SolidBrush background(MDCTheme::Background);

    graphics.FillRectangle(
        &background,
        0,
        0,
        g_width,
        g_height
    );

    DrawSidebar(graphics);

    int contentX = SIDEBAR_WIDTH + 24;
    int contentWidth = g_width - contentX - 28;

    DrawLabel(
        graphics,
        L"Storage overview",
        contentX,
        23,
        MDCTheme::Text,
        g_titleFont
    );

    DrawLabel(
        graphics,
        L"Analyze, review and manage your disk space.",
        contentX,
        59,
        MDCTheme::Secondary
    );

    DrawButton(
        graphics,
        R(g_width - 340, 23, 150, 43),
        L"Choose folder",
        false,
        g_hoverButton == 7
    );

    DrawButton(
        graphics,
        R(g_width - 174, 23, 145, 43),
        g_scanning ? L"Scanning..." : L"Scan folder",
        true,
        g_hoverButton == 6
    );

    DrawRoundRect(
        graphics,
        R(contentX, 91, contentWidth, 65),
        11,
        MDCTheme::Panel
    );

    DrawLabel(
        graphics,
        L"CURRENT FOLDER",
        contentX + 19,
        105,
        MDCTheme::Secondary,
        g_smallFont
    );

    std::wstring displayPath = g_drive;

    if (displayPath.length() > 90)
        displayPath = L"..." + displayPath.substr(displayPath.length() - 87);

    DrawLabel(
        graphics,
        displayPath,
        contentX + 19,
        128,
        MDCTheme::Text
    );

    int cardGap = 16;
    int cardWidth = (contentWidth - cardGap * 2) / 3;

    DrawStatCard(
        graphics,
        R(contentX, 173, cardWidth, 105),
        L"FILES DISCOVERED",
        std::to_wstring(GetFileCount()),
        L"Files found during scan",
        MDCTheme::Accent
    );

    DrawStatCard(
        graphics,
        R(contentX + cardWidth + cardGap, 173, cardWidth, 105),
        L"TOTAL SIZE",
        FormatSize(GetTotalSize()),
        L"Combined file size",
        MDCTheme::Green
    );

    std::wstring statusValue =
        g_scanning
        ? L"Scanning"
        : (g_scanFinished ? L"Complete" : L"Ready");

    DrawStatCard(
        graphics,
        R(contentX + (cardWidth + cardGap) * 2, 173,
            cardWidth, 105),
        L"SCAN STATUS",
        statusValue,
        g_scanning ? L"Please wait..." : L"Manual scan mode",
        MDCTheme::Orange
    );

    DrawLabel(
        graphics,
        L"FILE INVENTORY",
        contentX,
        300,
        MDCTheme::Text,
        g_titleFont
    );

    UiRect searchRect = R(g_width - 440, 291, 412, 35);

    DrawRoundRect(graphics, searchRect, 7, MDCTheme::Panel);
    DrawBorder(graphics, searchRect, 7, MDCTheme::Border);

    DrawLabel(
        graphics,
        g_search.empty() ? L"Search files..." : g_search,
        searchRect.x + 15,
        searchRect.y + 9,
        g_search.empty() ? MDCTheme::Muted : MDCTheme::Text
    );

    if (g_searchFocused) {
        Pen focusPen(MDCTheme::Accent, 1.0f);

        graphics.DrawRectangle(
            &focusPen,
            searchRect.x,
            searchRect.y,
            searchRect.w,
            searchRect.h
        );
    }

    DrawTable(graphics);

    int progressY = g_height - 48;

    DrawLabel(
        graphics,
        g_status,
        contentX,
        progressY - 1,
        MDCTheme::Secondary,
        g_smallFont
    );

    DrawProgress(
        graphics,
        R(g_width - 245, progressY, 215, 10)
    );

    Graphics output(hdc);
    output.DrawImage(&buffer, 0, 0);
}

int GetButtonAt(int x, int y) {
    if (R(16, 145, 194, 43).Contains(x, y)) return 0;
    if (R(16, 198, 194, 43).Contains(x, y)) return 1;
    if (R(16, 251, 194, 43).Contains(x, y)) return 2;
    if (R(16, 304, 194, 43).Contains(x, y)) return 3;

    if (R(16, 410, 194, 43).Contains(x, y)) return 4;
    if (R(16, 463, 194, 43).Contains(x, y)) return 5;
    if (R(16, 516, 194, 43).Contains(x, y)) return 8;

    if (R(g_width - 174, 23, 145, 43).Contains(x, y))
        return 6;

    if (R(g_width - 340, 23, 150, 43).Contains(x, y))
        return 7;

    return -1;
}

void HandleClick(int x, int y) {
    int button = GetButtonAt(x, y);

    if (button >= 0 && button <= 3) {
        g_page = (Page)button;
        RefreshVisibleFiles();
        InvalidateRect(g_hwnd, nullptr, FALSE);
        return;
    }

    if (button == 4) {
        ExportCSV();
        return;
    }

    if (button == 5) {
        DeleteSelected();
        return;
    }

    if (button == 6) {
        StartScan();
        return;
    }

    if (button == 7) {
        if (!g_scanning && ChooseFolder()) {
            g_status = L"Folder selected. Ready to scan.";
        }

        InvalidateRect(g_hwnd, nullptr, FALSE);
        return;
    }

    if (button == 8) {
        OpenFileLocation();
        return;
    }

    UiRect searchRect = R(g_width - 440, 291, 412, 35);

    if (searchRect.Contains(x, y)) {
        g_searchFocused = true;
        SetFocus(g_hwnd);
        InvalidateRect(g_hwnd, nullptr, FALSE);
        return;
    }

    g_searchFocused = false;

    int listY = 389;
    int rowHeight = 43;

    if (y >= listY && y < g_height - 75) {
        int index = (y - listY + g_scroll) / rowHeight;

        if (index >= 0 && index < (int)g_visibleFiles.size())
            g_selected = index;
        else
            g_selected = -1;
    }

    InvalidateRect(g_hwnd, nullptr, FALSE);
}

LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
) {
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        PaintInterface(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_GETMINMAXINFO: {
        auto* info = (MINMAXINFO*)lParam;

        info->ptMinTrackSize.x = WINDOW_MIN_WIDTH;
        info->ptMinTrackSize.y = WINDOW_MIN_HEIGHT;

        return 0;
    }

    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        int button = GetButtonAt(x, y);

        if (button != g_hoverButton) {
            g_hoverButton = button;
            InvalidateRect(hwnd, nullptr, FALSE);
        }

        return 0;
    }

    case WM_LBUTTONDOWN:
        HandleClick(
            GET_X_LPARAM(lParam),
            GET_Y_LPARAM(lParam)
        );
        return 0;

    case WM_LBUTTONDBLCLK: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        int listY = 389;
        int rowHeight = 43;

        if (y >= listY && y < g_height - 75) {
            int index = (y - listY + g_scroll) / rowHeight;

            if (index >= 0 &&
                index < (int)g_visibleFiles.size()) {
                g_selected = index;
                OpenFileLocation();
            }
        }

        return 0;
    }

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);

        g_scroll -= delta / 2;

        int maxScroll = std::max(
            0,
            (int)g_visibleFiles.size() * 43 -
            (g_height - 389 - 85)
        );

        g_scroll = std::clamp(g_scroll, 0, maxScroll);

        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_CHAR:
        if (g_searchFocused) {
            if (wParam == VK_BACK) {
                if (!g_search.empty())
                    g_search.pop_back();
            }
            else if (wParam >= 32) {
                g_search.push_back((wchar_t)wParam);
            }

            RefreshVisibleFiles();
            InvalidateRect(hwnd, nullptr, FALSE);
        }

        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            g_searchFocused = false;
            g_search.clear();

            RefreshVisibleFiles();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        else if (wParam == VK_DELETE) {
            DeleteSelected();
        }

        return 0;

    case WM_TIMER:
        g_animation += 0.08f;

        if (g_animation > 6.283185f)
            g_animation = 0.0f;

        if (g_scanning)
            InvalidateRect(hwnd, nullptr, FALSE);

        return 0;

    case WM_SCAN_PROGRESS:
        g_status =
            L"Scanning... " +
            std::to_wstring((size_t)wParam) +
            L" files discovered.";

        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_SCAN_FINISHED: {
        std::unique_ptr<std::vector<FileEntry>> result(
            (std::vector<FileEntry>*)lParam
        );

        if (result) {
            {
                std::lock_guard<std::mutex> lock(g_filesMutex);
                g_files = std::move(*result);
            }

            g_scanning = false;
            g_scanFinished = true;
            g_progress = 100;

            RefreshVisibleFiles();

            g_status =
                L"Scan complete. " +
                std::to_wstring(GetFileCount()) +
                L" files discovered.";
        }

        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_DESTROY:
        g_cancelScan = true;
        g_scanning = false;

        KillTimer(hwnd, 1);

        if (g_worker.joinable())
            g_worker.join();

        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int show
) {
    GdiplusStartupInput startupInput;

    if (GdiplusStartup(
        &g_gdiplusToken,
        &startupInput,
        nullptr
    ) != Ok) {
        MessageBoxW(
            nullptr,
            L"Could not initialize GDI+.",
            L"MDC",
            MB_OK | MB_ICONERROR
        );

        return 1;
    }

    g_font = new Font(
        L"Segoe UI",
        10.0f,
        FontStyleRegular,
        UnitPixel
    );

    g_smallFont = new Font(
        L"Segoe UI",
        9.0f,
        FontStyleRegular,
        UnitPixel
    );

    g_titleFont = new Font(
        L"Segoe UI",
        19.0f,
        FontStyleBold,
        UnitPixel
    );

    WNDCLASSEXW windowClass{};

    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = (HICON)LoadImageW(
        instance,
        MAKEINTRESOURCEW(101),
        IMAGE_ICON,
        0,
        0,
        LR_DEFAULTSIZE
    );
    windowClass.hIconSm = windowClass.hIcon;
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = L"MDC_MAIN_WINDOW";

    if (!RegisterClassExW(&windowClass)) {
        MessageBoxW(
            nullptr,
            L"Could not register application window.",
            L"MDC",
            MB_OK | MB_ICONERROR
        );

        GdiplusShutdown(g_gdiplusToken);
        return 1;
    }

    g_hwnd = CreateWindowExW(
        0,
        windowClass.lpszClassName,
        L"MDC - Storage Intelligence",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1280,
        820,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (!g_hwnd) {
        MessageBoxW(
            nullptr,
            L"Could not create application window.",
            L"MDC",
            MB_OK | MB_ICONERROR
        );

        GdiplusShutdown(g_gdiplusToken);
        return 1;
    }

    ShowWindow(g_hwnd, show);
    UpdateWindow(g_hwnd);

    SetTimer(g_hwnd, 1, 16, nullptr);

    MSG message{};

    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    delete g_font;
    delete g_smallFont;
    delete g_titleFont;

    GdiplusShutdown(g_gdiplusToken);

    return (int)message.wParam;
}
