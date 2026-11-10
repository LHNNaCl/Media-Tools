#pragma once

#include <optional>
#include <string>
#include <vector>

namespace MediaTool {

enum class Feature { Info, ExtractAudio, ExtractSubtitle, MuxSubtitle, Trim, Merge, Convert, Audio };
enum class ItemStatus { Succeeded, Failed, Canceled, NotProcessed };
enum class AudioFormat { Mp3, M4a, Wav, Flac };
enum class SubtitleFormat { Srt, Ass, Vtt, Raw };
enum class ContainerKind { Mp4, Mkv, Webm };
enum class ConvertMode { Remux, Compress };
enum class ResolutionPreset { Original, P1080, P720 };
enum class VideoEncoder { H264, H265, Vp9 };
enum class EncoderImpl { Nvenc, Qsv, Amf, Cpu };
enum class AudioEditMode { Remove, Keep, Replace };
enum class TrimMode { StartEnd, StartDuration };
enum class SubtitleKind { Text, Image };
enum class StepKind { Ffmpeg, WriteText };
enum class CancelChoice { StopAll, SkipCurrent, Continue };

struct AudioTrack {
    int number = 0;
    std::string codec;
    int channels = 0;
    std::optional<int> sampleRate;
    std::optional<std::string> languageTag;
};

struct SubtitleTrack {
    int number = 0;
    std::string codec;
    std::optional<std::string> languageTag;
    SubtitleKind kind = SubtitleKind::Image;
};

struct ChapterMark {
    double startSeconds = 0;
    std::optional<std::string> title;
};

struct FitStream {
    int absoluteIndex = 0;
    std::string codecType;
    std::string codecName;
};

struct MediaInfo {
    std::string path;
    std::optional<double> durationSeconds;
    int width = 0;
    int height = 0;
    std::optional<int> frameNumerator;
    std::optional<int> frameDenominator;
    std::string videoCodec;
    std::optional<long long> videoBitrateBits;
    std::optional<std::string> pixelFormat;
    std::vector<AudioTrack> audios;
    std::vector<SubtitleTrack> subtitles;
    std::vector<ChapterMark> chapters;
    std::vector<FitStream> fitStreams;
};

struct ProcessStep {
    StepKind kind = StepKind::Ffmpeg;
    std::vector<std::string> arguments;
    std::string outputPath;
    std::optional<std::string> textContent;
    double durationSeconds = 0;
};

struct ExecutionPlan {
    std::vector<ProcessStep> steps;
    std::string successMessage;
    bool usesHardware = false;
};

struct PlanResult {
    std::optional<ExecutionPlan> plan;
    std::optional<std::string> failureMessage;
};

struct EncoderAvailability {
    bool h264Nvenc = false;
    bool h264Qsv = false;
    bool h264Amf = false;
    bool h265Nvenc = false;
    bool h265Qsv = false;
    bool h265Amf = false;
    bool vp9Qsv = false;
};

struct ItemResult {
    std::string inputName;
    ItemStatus status = ItemStatus::Failed;
    std::string message;
    std::string outputPath;
};

struct PlanRequest {
    Feature feature = Feature::Info;
    MediaInfo media;
    std::vector<MediaInfo> mergeInputs;
    std::string outputPath;
    std::string tempDir;
    int trackNumber = 1;
    AudioFormat audioFormat = AudioFormat::Mp3;
    int volume = 100;
    SubtitleFormat subtitleFormat = SubtitleFormat::Srt;
    std::string externalPath;
    TrimMode trimMode = TrimMode::StartEnd;
    std::string trimStart;
    std::string trimEnd;
    std::string trimDuration;
    ContainerKind container = ContainerKind::Mp4;
    ConvertMode convertMode = ConvertMode::Remux;
    ResolutionPreset resolution = ResolutionPreset::Original;
    VideoEncoder videoEncoder = VideoEncoder::H264;
    std::optional<int> bitrateKbps;
    AudioEditMode audioEdit = AudioEditMode::Remove;
    EncoderImpl encoderImpl = EncoderImpl::Cpu;
};

}  // namespace MediaTool
