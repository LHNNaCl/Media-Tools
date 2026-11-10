#pragma once

#include "mediatool/types.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MediaTool {

std::filesystem::path utf8Path(std::string_view utf8);
std::string toUtf8(const std::filesystem::path& path);
std::string fileNameOf(std::string_view path);
std::string stemOf(std::string_view path);
std::string extensionOf(std::string_view path);
std::string parentOf(std::string_view path);
std::string lowerCopy(std::string text);

std::optional<double> parseClock(std::string text);
bool clockExceeds(double endSeconds, double durationSeconds);

struct ResolvedTrim {
    double start = 0;
    double duration = 0;
};

std::optional<ResolvedTrim> resolveTrim(TrimMode mode, const std::string& startText, const std::string& endOrDuration,
                                        std::optional<double> mediaDuration, std::string& error);

std::string formatClock(double seconds);
std::string formatInfo(const MediaInfo& info);

std::string allocateOutput(const std::string& directory, const std::string& stem, const std::string& suffix,
                           const std::string& extension);
bool isSamePath(const std::string& left, const std::string& right);
bool directoryWritable(const std::string& directory);

std::vector<std::string> naturalSort(std::vector<std::string> paths);
int naturalCompare(std::string_view left, std::string_view right);

std::optional<std::string> findSidecar(const std::string& videoPath, const std::vector<std::string>& extensions);

bool containerAccepts(ContainerKind target, const std::vector<FitStream>& streams);
bool containerAcceptsAudio(ContainerKind target, const std::string& audioCodec);

std::optional<std::string> compressScale(int width, int height, ResolutionPreset preset);
std::pair<int, int> evenSize(int width, int height);

std::optional<std::string> volumeFilter(int volumePercent);
EncoderImpl selectEncoder(VideoEncoder family, const EncoderAvailability& available);
std::vector<std::string> videoEncodeArgs(VideoEncoder family, EncoderImpl impl, std::optional<int> bitrateKbps);

SubtitleKind subtitleKindOf(const std::string& codecName);
bool fileNonEmpty(const std::string& path);
std::string containerExtension(ContainerKind kind);
ContainerKind containerFromExtension(const std::string& extension);

std::vector<std::string> ffmpegPrefix();
void appendFaststartIfNeeded(std::vector<std::string>& args, const std::string& outputPath);

PlanResult planJob(const PlanRequest& request);
std::string outputSuffix(Feature feature, ConvertMode convertMode, AudioEditMode audioEdit);
std::string outputExtensionFor(const PlanRequest& request);

}  // namespace MediaTool
