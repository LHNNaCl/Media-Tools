#include "mediatool/logic.hpp"
#include "mediatool/messages.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

namespace MediaTool {
namespace {

bool iequals(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) return false;
    for (size_t i = 0; i < left.size(); ++i) {
        const auto a = static_cast<unsigned char>(left[i]);
        const auto b = static_cast<unsigned char>(right[i]);
        if (std::tolower(a) != std::tolower(b)) return false;
    }
    return true;
}

std::string trimCopy(std::string text) {
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), notSpace));
    text.erase(std::find_if(text.rbegin(), text.rend(), notSpace).base(), text.end());
    return text;
}

bool parsePart(const std::string& text, int& value) {
    if (text.empty() || !std::all_of(text.begin(), text.end(), [](unsigned char ch) { return std::isdigit(ch); })) {
        return false;
    }
    try {
        value = std::stoi(text);
    } catch (...) {
        return false;
    }
    return value >= 0;
}

}  // namespace

std::filesystem::path utf8Path(std::string_view utf8) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

std::string toUtf8(const std::filesystem::path& path) {
    const auto u8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

std::string fileNameOf(std::string_view path) { return toUtf8(utf8Path(path).filename()); }

std::string stemOf(std::string_view path) { return toUtf8(utf8Path(path).stem()); }

std::string extensionOf(std::string_view path) { return lowerCopy(toUtf8(utf8Path(path).extension())); }

std::string parentOf(std::string_view path) { return toUtf8(utf8Path(path).parent_path()); }

std::string lowerCopy(std::string text) {
    for (char& ch : text) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return text;
}

std::optional<double> parseClock(std::string text) {
    text = trimCopy(std::move(text));
    if (text.empty()) return std::nullopt;
    static const std::regex secondsOnly(R"(^(\d+)$)");
    static const std::regex minuteSecond(R"(^(\d+):([0-5]?\d)$)");
    static const std::regex hourMinuteSecond(R"(^(\d+):([0-5]?\d):([0-5]?\d)$)");
    std::smatch match;
    if (std::regex_match(text, match, secondsOnly)) return std::stod(match[1].str());
    if (std::regex_match(text, match, minuteSecond)) {
        return std::stod(match[1].str()) * 60.0 + std::stod(match[2].str());
    }
    if (std::regex_match(text, match, hourMinuteSecond)) {
        return std::stod(match[1].str()) * 3600.0 + std::stod(match[2].str()) * 60.0 + std::stod(match[3].str());
    }
    return std::nullopt;
}

bool clockExceeds(double endSeconds, double durationSeconds) { return endSeconds > durationSeconds + 0.001; }

std::optional<ResolvedTrim> resolveTrim(TrimMode mode, const std::string& startText, const std::string& endOrDuration,
                                        std::optional<double> mediaDuration, std::string& error) {
    const auto start = parseClock(startText);
    const auto other = parseClock(endOrDuration);
    if (!start || !other || *start < 0 || *other < 0) {
        error = kBadTime;
        return std::nullopt;
    }
    double end = *other;
    if (mode == TrimMode::StartDuration) {
        if (*other <= 0) {
            error = kBadTime;
            return std::nullopt;
        }
        end = *start + *other;
    }
    if (!(end > *start)) {
        error = kBadTime;
        return std::nullopt;
    }
    if (mediaDuration && clockExceeds(end, *mediaDuration)) {
        error = kTimePastEnd;
        return std::nullopt;
    }
    ResolvedTrim resolved;
    resolved.start = *start;
    resolved.duration = end - *start;
    return resolved;
}

std::string formatClock(double seconds) {
    if (seconds < 0) seconds = 0;
    const long long totalMs = std::llround(seconds * 1000.0);
    const long long hours = totalMs / 3600000;
    const long long minutes = (totalMs / 60000) % 60;
    const long long secs = (totalMs / 1000) % 60;
    const long long millis = totalMs % 1000;
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setfill('0') << std::setw(2) << hours << ':' << std::setw(2) << minutes << ':' << std::setw(2) << secs
        << '.' << std::setw(3) << millis;
    return out.str();
}

std::string formatInfo(const MediaInfo& info) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "时长：" << (info.durationSeconds ? formatClock(*info.durationSeconds) : "未知") << '\n';
    out << "分辨率：" << info.width << "×" << info.height << '\n';
    if (!info.frameNumerator || !info.frameDenominator || *info.frameDenominator == 0) {
        out << "帧率：未知\n";
    } else {
        const double fps = static_cast<double>(*info.frameNumerator) / static_cast<double>(*info.frameDenominator);
        const double nearest = std::round(fps);
        out << "帧率：";
        if (std::abs(fps - nearest) < 0.0005) {
            out << static_cast<long long>(nearest);
        } else {
            out << std::fixed << std::setprecision(3) << fps;
        }
        out << '\n';
    }
    out << "视频编码：" << info.videoCodec << '\n';
    if (!info.videoBitrateBits || *info.videoBitrateBits <= 0) {
        out << "视频码率：未知\n";
    } else {
        out << "视频码率：" << std::llround(static_cast<double>(*info.videoBitrateBits) / 1000.0) << " kbps\n";
    }
    out << "音轨：\n";
    if (info.audios.empty()) {
        out << "无音轨\n";
    } else {
        for (const auto& track : info.audios) {
            const auto& tag = track.languageTag;
            const char* language = (!tag || tag->empty() || iequals(*tag, "und")) ? "未标记" : tag->c_str();
            out << track.number << ". " << track.codec << "，" << track.channels << " 声道，" << language << '\n';
        }
    }
    out << "字幕：\n";
    if (info.subtitles.empty()) {
        out << "无字幕\n";
    } else {
        for (const auto& track : info.subtitles) {
            const auto& tag = track.languageTag;
            const char* language = (!tag || tag->empty() || iequals(*tag, "und")) ? "未标记" : tag->c_str();
            out << track.number << ". " << track.codec << "，" << language << "，"
                << (track.kind == SubtitleKind::Text ? "文本" : "图像") << '\n';
        }
    }
    out << "章节：\n";
    if (info.chapters.empty()) {
        out << "无章节\n";
    } else {
        for (const auto& chapter : info.chapters) {
            out << formatClock(chapter.startSeconds);
            if (chapter.title && !chapter.title->empty()) out << ' ' << *chapter.title;
            out << '\n';
        }
    }
    return out.str();
}

std::string allocateOutput(const std::string& directory, const std::string& stem, const std::string& suffix,
                           const std::string& extension) {
    auto candidate = [&](const std::string& extra) {
        return toUtf8(utf8Path(directory) / utf8Path(stem + suffix + extra + extension));
    };
    std::string first = candidate("");
    if (!std::filesystem::exists(utf8Path(first))) return first;
    for (int number = 2;; ++number) {
        std::string next = candidate(" (" + std::to_string(number) + ")");
        if (!std::filesystem::exists(utf8Path(next))) return next;
    }
}

bool isSamePath(const std::string& left, const std::string& right) {
    std::error_code error;
    return std::filesystem::equivalent(utf8Path(left), utf8Path(right), error);
}

bool directoryWritable(const std::string& directory) {
    const auto probe = utf8Path(directory) / ".mediatool-write-test";
    std::ofstream stream(probe, std::ios::binary);
    if (!stream) return false;
    stream << "ok";
    stream.close();
    std::error_code error;
    std::filesystem::remove(probe, error);
    return true;
}

namespace {

struct SortToken {
    bool numeric = false;
    std::string text;
    unsigned long long value = 0;
};

std::vector<SortToken> tokenize(std::string_view name) {
    std::vector<SortToken> tokens;
    size_t index = 0;
    while (index < name.size()) {
        const bool digit = std::isdigit(static_cast<unsigned char>(name[index]));
        size_t end = index + 1;
        while (end < name.size() && (std::isdigit(static_cast<unsigned char>(name[end])) != 0) == digit) ++end;
        SortToken token;
        token.numeric = digit;
        token.text = std::string(name.substr(index, end - index));
        if (digit) {
            try {
                token.value = std::stoull(token.text);
            } catch (...) {
                token.value = 0;
            }
        }
        tokens.push_back(std::move(token));
        index = end;
    }
    return tokens;
}

}  // namespace

int naturalCompare(std::string_view left, std::string_view right) {
    const auto a = tokenize(left);
    const auto b = tokenize(right);
    const size_t count = std::min(a.size(), b.size());
    for (size_t i = 0; i < count; ++i) {
        if (a[i].numeric && b[i].numeric && a[i].value != b[i].value) return a[i].value < b[i].value ? -1 : 1;
        if (a[i].numeric != b[i].numeric) return a[i].numeric ? -1 : 1;
        if (!a[i].numeric && !iequals(a[i].text, b[i].text)) return lowerCopy(a[i].text) < lowerCopy(b[i].text) ? -1 : 1;
    }
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    if (!iequals(left, right)) return lowerCopy(std::string(left)) < lowerCopy(std::string(right)) ? -1 : 1;
    if (left == right) return 0;
    return left < right ? -1 : 1;
}

std::vector<std::string> naturalSort(std::vector<std::string> paths) {
    std::sort(paths.begin(), paths.end(), [](const std::string& left, const std::string& right) {
        return naturalCompare(fileNameOf(left), fileNameOf(right)) < 0;
    });
    return paths;
}

std::optional<std::string> findSidecar(const std::string& videoPath, const std::vector<std::string>& extensions) {
    const auto video = utf8Path(videoPath);
    const auto stem = lowerCopy(toUtf8(video.stem()));
    std::error_code error;
    if (!std::filesystem::is_directory(video.parent_path(), error)) return std::nullopt;
    for (const auto& extension : extensions) {
        for (const auto& entry : std::filesystem::directory_iterator(video.parent_path(), error)) {
            if (error || !entry.is_regular_file()) continue;
            if (lowerCopy(toUtf8(entry.path().stem())) == stem &&
                lowerCopy(toUtf8(entry.path().extension())) == lowerCopy(extension)) {
                return toUtf8(entry.path());
            }
        }
    }
    return std::nullopt;
}

bool containerAccepts(ContainerKind target, const std::vector<FitStream>& streams) {
    auto is = [](const std::string& value, std::initializer_list<const char*> allowed) {
        for (const char* item : allowed) {
            if (iequals(value, item)) return true;
        }
        return false;
    };
    for (const auto& stream : streams) {
        if (iequals(stream.codecType, "attachment") || iequals(stream.codecType, "data")) continue;
        if (target == ContainerKind::Mkv) continue;
        if (target == ContainerKind::Mp4) {
            if (iequals(stream.codecType, "video") && !is(stream.codecName, {"h264", "hevc"})) return false;
            if (iequals(stream.codecType, "audio") && !is(stream.codecName, {"aac", "mp3", "ac3", "eac3"})) return false;
            if (iequals(stream.codecType, "subtitle") && !is(stream.codecName, {"mov_text"})) return false;
            continue;
        }
        if (iequals(stream.codecType, "video") && !is(stream.codecName, {"vp8", "vp9", "av1"})) return false;
        if (iequals(stream.codecType, "audio") && !is(stream.codecName, {"vorbis", "opus"})) return false;
        if (iequals(stream.codecType, "subtitle") && !is(stream.codecName, {"webvtt"})) return false;
    }
    return true;
}

bool containerAcceptsAudio(ContainerKind target, const std::string& audioCodec) {
    FitStream stream;
    stream.codecType = "audio";
    stream.codecName = audioCodec;
    return containerAccepts(target, {stream});
}

std::optional<std::string> compressScale(int width, int height, ResolutionPreset preset) {
    auto even = [&]() -> std::optional<std::string> {
        if (width % 2 || height % 2) return "scale=trunc(iw/2)*2:trunc(ih/2)*2";
        return std::nullopt;
    };
    if (preset == ResolutionPreset::Original) return even();
    const int target = preset == ResolutionPreset::P1080 ? 1080 : 720;
    if (height > target) return "scale=-2:" + std::to_string(target);
    return even();
}

std::pair<int, int> evenSize(int width, int height) {
    int evenWidth = width - width % 2;
    int evenHeight = height - height % 2;
    if (evenWidth < 2) evenWidth = 2;
    if (evenHeight < 2) evenHeight = 2;
    return {evenWidth, evenHeight};
}

std::optional<std::string> volumeFilter(int volumePercent) {
    if (volumePercent == 100) return std::nullopt;
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(2) << (static_cast<double>(volumePercent) / 100.0);
    return "volume=" + out.str();
}

EncoderImpl selectEncoder(VideoEncoder family, const EncoderAvailability& available) {
    if (family == VideoEncoder::H264) {
        if (available.h264Nvenc) return EncoderImpl::Nvenc;
        if (available.h264Qsv) return EncoderImpl::Qsv;
        if (available.h264Amf) return EncoderImpl::Amf;
        return EncoderImpl::Cpu;
    }
    if (family == VideoEncoder::H265) {
        if (available.h265Nvenc) return EncoderImpl::Nvenc;
        if (available.h265Qsv) return EncoderImpl::Qsv;
        if (available.h265Amf) return EncoderImpl::Amf;
        return EncoderImpl::Cpu;
    }
    if (available.vp9Qsv) return EncoderImpl::Qsv;
    return EncoderImpl::Cpu;
}

std::vector<std::string> videoEncodeArgs(VideoEncoder family, EncoderImpl impl, std::optional<int> bitrateKbps) {
    const bool vp9 = family == VideoEncoder::Vp9;
    const std::string codec = [&] {
        if (impl == EncoderImpl::Nvenc) return family == VideoEncoder::H265 ? "hevc_nvenc" : "h264_nvenc";
        if (impl == EncoderImpl::Qsv) return vp9 ? "vp9_qsv" : family == VideoEncoder::H265 ? "hevc_qsv" : "h264_qsv";
        if (impl == EncoderImpl::Amf) return family == VideoEncoder::H265 ? "hevc_amf" : "h264_amf";
        return vp9 ? "libvpx-vp9" : family == VideoEncoder::H265 ? "libx265" : "libx264";
    }();
    std::vector<std::string> args{"-c:v", codec};
    if (impl == EncoderImpl::Nvenc) {
        args.insert(args.end(), {"-preset", "p5", "-rc", "vbr"});
        if (bitrateKbps) args.insert(args.end(), {"-b:v", std::to_string(*bitrateKbps) + "k"});
        else args.insert(args.end(), {"-cq", "23", "-b:v", "0"});
    } else if (impl == EncoderImpl::Qsv) {
        args.insert(args.end(), {"-preset", "medium"});
        if (bitrateKbps) args.insert(args.end(), {"-b:v", std::to_string(*bitrateKbps) + "k"});
        else args.insert(args.end(), {"-global_quality", vp9 ? "32" : "23"});
    } else if (impl == EncoderImpl::Amf) {
        args.insert(args.end(), {"-quality", "balanced"});
        if (bitrateKbps) args.insert(args.end(), {"-rc", "cbr", "-b:v", std::to_string(*bitrateKbps) + "k"});
        else args.insert(args.end(), {"-rc", "cqp", "-qp_i", "23", "-qp_p", "23"});
    } else if (vp9) {
        args.insert(args.end(), {"-deadline", "good", "-cpu-used", "2"});
        if (bitrateKbps) args.insert(args.end(), {"-b:v", std::to_string(*bitrateKbps) + "k"});
        else args.insert(args.end(), {"-crf", "32", "-b:v", "0"});
    } else {
        args.insert(args.end(), {"-preset", "medium"});
        if (bitrateKbps) args.insert(args.end(), {"-b:v", std::to_string(*bitrateKbps) + "k"});
        else args.insert(args.end(), {"-crf", "23"});
    }
    return args;
}

SubtitleKind subtitleKindOf(const std::string& codecName) {
    const auto codec = lowerCopy(codecName);
    if (codec == "subrip" || codec == "ass" || codec == "ssa" || codec == "webvtt" || codec == "mov_text" ||
        codec == "text") {
        return SubtitleKind::Text;
    }
    return SubtitleKind::Image;
}

bool fileNonEmpty(const std::string& path) {
    std::error_code error;
    const auto file = utf8Path(path);
    if (!std::filesystem::is_regular_file(file, error)) return false;
    std::ifstream stream(file, std::ios::binary);
    if (!stream) return false;
    stream.seekg(0, std::ios::end);
    return stream.tellg() > 0;
}

std::string containerExtension(ContainerKind kind) {
    switch (kind) {
    case ContainerKind::Mp4: return ".mp4";
    case ContainerKind::Mkv: return ".mkv";
    case ContainerKind::Webm: return ".webm";
    }
    return ".mp4";
}

ContainerKind containerFromExtension(const std::string& extension) {
    const auto ext = lowerCopy(extension);
    if (ext == ".mkv") return ContainerKind::Mkv;
    if (ext == ".webm") return ContainerKind::Webm;
    if (ext == ".avi") return ContainerKind::Mp4;
    return ContainerKind::Mp4;
}

std::vector<std::string> ffmpegPrefix() {
    return {"-hide_banner", "-loglevel", "error", "-progress", "pipe:1", "-nostats"};
}

void appendFaststartIfNeeded(std::vector<std::string>& args, const std::string& outputPath) {
    const auto ext = extensionOf(outputPath);
    if (ext == ".mp4" || ext == ".m4a" || ext == ".mov") args.insert(args.end(), {"-movflags", "+faststart"});
}

}  // namespace MediaTool
