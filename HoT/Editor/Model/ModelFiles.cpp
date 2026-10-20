#include "HoT/Editor/Model/ModelFiles.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdio>
#include <cstring>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "HoT/Streaming/LooseFiles.h"

namespace HoT::Editor::Model::Files {

std::wstring Widen(const std::string& text) {
    if (text.empty()) {
        return std::wstring();
    }

    int32_t length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int32_t>(text.size()), nullptr, 0);

    if (length <= 0) {
        return std::wstring();
    }

    std::wstring result(length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int32_t>(text.size()), &result[0], length);
    return result;
}

std::string Narrow(const std::wstring& text) {
    if (text.empty()) {
        return std::string();
    }

    int32_t length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int32_t>(text.size()), nullptr, 0, nullptr, nullptr);

    if (length <= 0) {
        return std::string();
    }

    std::string result(length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int32_t>(text.size()), &result[0], length, nullptr, nullptr);
    return result;
}

std::wstring GameDirectory() {
    wchar_t buffer[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    std::wstring path(buffer, length);
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

std::wstring DiskPath(const std::string& gamePath) {
    return GameDirectory() + L"\\" + Widen(gamePath);
}

bool NormalizeGamePath(const std::string& input, std::string& output, std::string& error) {
    size_t first = input.find_first_not_of(" \t");
    size_t last = input.find_last_not_of(" \t");

    if (first == std::string::npos) {
        error = "empty path";
        return false;
    }

    std::string path;

    for (size_t i = first; i <= last; i++) {
        char c = input[i] == '/' ? '\\' : input[i];

        if (static_cast<unsigned char>(c) < 32 || strchr("<>\"|?*:", c)) {
            error = "invalid character in path";
            return false;
        }

        if (c == '\\' && (path.empty() || path.back() == '\\')) {
            if (path.empty()) {
                error = "path must be relative to the game folder";
                return false;
            }

            continue;
        }

        path += c;
    }

    if (path.empty() || path.back() == '\\') {
        error = "path names a folder";
        return false;
    }

    if (path.size() > 200) {
        error = "path is too long";
        return false;
    }

    size_t start = 0;

    while (start <= path.size()) {
        size_t end = path.find('\\', start);
        std::string part = path.substr(start, end == std::string::npos ? std::string::npos : end - start);

        if (part == "." || part == "..") {
            error = "path must not contain . or ..";
            return false;
        }

        if (end == std::string::npos) {
            break;
        }

        start = end + 1;
    }

    output = path;
    return true;
}

std::string Lower(const std::string& text) {
    std::string result = text;

    for (char& c : result) {
        if (c == '/') {
            c = '\\';
        } else if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }

    return result;
}

bool EndsWith(const std::string& text, const char* suffix) {
    size_t length = strlen(suffix);
    return text.size() >= length && _stricmp(text.c_str() + text.size() - length, suffix) == 0;
}

std::string StripExtension(const std::string& path) {
    size_t dot = path.find_last_of('.');
    size_t slash = path.find_last_of("\\/");

    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
        return path;
    }

    return path.substr(0, dot);
}

std::string FileStem(const std::string& path) {
    std::string base = StripExtension(path);
    size_t slash = base.find_last_of("\\/");
    return slash == std::string::npos ? base : base.substr(slash + 1);
}

std::string Folder(const std::string& path) {
    size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

bool GameFileExists(const std::string& gamePath) {
    return SFile::FileExistsEx(gamePath.c_str(), 0) != 0;
}

bool IsLooseFile(const std::string& gamePath) {
    DWORD attributes = GetFileAttributesW(DiskPath(gamePath).c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool ReadGameFile(const std::string& gamePath, std::vector<uint8_t>& data, uint32_t maxSize) {
    SFileImpl* file = nullptr;

    if (!SFile::OpenEx(nullptr, gamePath.c_str(), 0, &file) || !file) {
        return false;
    }

    uint32_t size = SFileGetFileSize(file, nullptr);

    if (size > maxSize) {
        SFileCloseFile(file);
        return false;
    }

    data.resize(size);
    bool ok = !size || SFileReadFile(file, data.data(), size, nullptr, nullptr, 0) != 0;
    SFileCloseFile(file);

    if (!ok) {
        data.clear();
    }

    return ok;
}

static void CreateDirectories(const std::wstring& path) {
    for (size_t i = 3; i <= path.size(); i++) {
        if (i == path.size() || path[i] == L'\\') {
            CreateDirectoryW(path.substr(0, i).c_str(), nullptr);
        }
    }
}

bool WriteLoose(const std::string& gamePath, const void* data, size_t size, std::string& error) {
    std::wstring path = DiskPath(gamePath);
    size_t slash = path.find_last_of(L'\\');

    if (slash != std::wstring::npos) {
        CreateDirectories(path.substr(0, slash));
    }

    DWORD attributes = GetFileAttributesW(path.c_str());

    if (attributes != INVALID_FILE_ATTRIBUTES) {
        if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
            error = gamePath + " is a folder";
            return false;
        }

        std::wstring backup = path + L".bak";

        if (GetFileAttributesW(backup.c_str()) == INVALID_FILE_ATTRIBUTES && !CopyFileW(path.c_str(), backup.c_str(), TRUE)) {
            error = "could not back up " + gamePath;
            return false;
        }
    }

    std::wstring temp = path + L".tmp";
    FILE* file = _wfopen(temp.c_str(), L"wb");

    if (!file) {
        error = "could not write " + gamePath;
        return false;
    }

    bool ok = !size || fwrite(data, 1, size, file) == size;
    ok = fclose(file) == 0 && ok;

    if (!ok || !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(temp.c_str());
        error = "could not write " + gamePath;
        return false;
    }

    return true;
}

void RefreshLooseIndex() {
    Streaming::LooseFiles::Invalidate();
}

}
