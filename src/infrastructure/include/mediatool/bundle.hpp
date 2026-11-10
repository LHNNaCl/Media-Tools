#pragma once

#include <filesystem>
#include <string>

namespace MediaTool {

struct Bundle {
    std::filesystem::path root;
    std::filesystem::path ffmpeg;
    std::filesystem::path ffprobe;
    std::filesystem::path licenseApp;
    std::filesystem::path licenseFfmpeg;
    std::filesystem::path sourceNote;
    bool valid = false;
};

Bundle locateBundle(const std::filesystem::path& executableDirectory);

}  // namespace MediaTool
