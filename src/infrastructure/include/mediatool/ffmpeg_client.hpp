#pragma once

#include "mediatool/types.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace MediaTool {

std::optional<MediaInfo> probeMedia(const std::string& ffprobe, const std::string& path, std::string& error);
int runFfmpeg(const std::string& ffmpeg, const std::vector<std::string>& arguments, const std::function<void(int)>& percent,
              const std::function<bool()>& canceled, double durationSeconds);
std::string ffmpegVersion(const std::string& ffprobe);
bool probeEncoders(const std::string& ffmpeg, EncoderAvailability& available, const std::function<void(const std::string&)>& status);

}  // namespace MediaTool
