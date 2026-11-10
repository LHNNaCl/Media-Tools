#include "test_harness.hpp"

#include "mediatool/logic.hpp"
#include "mediatool/messages.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace {
using namespace MediaTool;

bool hasToken(const std::vector<std::string>& args, const std::string& token) {
    for (const auto& arg : args) {
        if (arg == token) return true;
    }
    return false;
}

struct TempDir {
    std::filesystem::path path;
    explicit TempDir(const char* name) {
        path = std::filesystem::temp_directory_path() / name;
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
    }
    ~TempDir() { std::filesystem::remove_all(path); }
    std::string str() const { return toUtf8(path); }
    std::string file(const std::string& name, const std::string& text = "x") const {
        const auto full = path / std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(name.data()), name.size()));
        std::ofstream stream(full, std::ios::binary);
        stream << text;
        return toUtf8(full);
    }
};

MediaInfo clip(const std::string& path, const std::string& codec = "h264") {
    MediaInfo info;
    info.path = path;
    info.durationSeconds = 2;
    info.width = 320;
    info.height = 240;
    info.frameNumerator = 30;
    info.frameDenominator = 1;
    info.videoCodec = codec;
    info.pixelFormat = "yuv420p";
    return info;
}

FitStream stream(const char* type, const char* codec) {
    FitStream item;
    item.codecType = type;
    item.codecName = codec;
    return item;
}
}  // namespace

TEST(UT_01) {
    const auto value = parseClock("90");
    EXPECT(value.has_value());
    EXPECT(*value == 90);
}

TEST(UT_02) {
    const auto value = parseClock("1:30");
    EXPECT(value.has_value());
    EXPECT(*value == 90);
}

TEST(UT_03) {
    const auto padded = parseClock("1:02:03");
    const auto shortForm = parseClock("1:2:3");
    EXPECT(padded.has_value() && shortForm.has_value());
    EXPECT(*padded == 3723);
    EXPECT(*shortForm == 3723);
}

TEST(UT_04) {
    EXPECT(!parseClock("1:60"));
    EXPECT(!parseClock(""));
    EXPECT(!parseClock("1.5"));
    std::string error;
    EXPECT(!resolveTrim(TrimMode::StartEnd, "1:60", "2", 10, error));
    EXPECT(error == kBadTime);
}

TEST(UT_05) {
    std::string error;
    const auto range = resolveTrim(TrimMode::StartDuration, "10", "15", 60, error);
    EXPECT(range.has_value());
    EXPECT(range->start == 10);
    EXPECT(range->duration == 15);
    EXPECT(range->start + range->duration == 25);
}

TEST(UT_06) { EXPECT(clockExceeds(60.002, 60)); }

TEST(UT_07) { EXPECT(!clockExceeds(60.0005, 60)); }

TEST(UT_08) {
    TempDir dir("mediatool-ut08");
    const auto first = allocateOutput(dir.str(), "clip", "_audio", ".mp3");
    EXPECT(fileNameOf(first) == "clip_audio.mp3");
}

TEST(UT_09) {
    TempDir dir("mediatool-ut09");
    dir.file("clip_audio.mp3");
    const auto next = allocateOutput(dir.str(), "clip", "_audio", ".mp3");
    EXPECT(fileNameOf(next) == "clip_audio (2).mp3");
}

TEST(UT_10) { EXPECT(naturalCompare("clip2.mp4", "clip10.mp4") < 0); }

TEST(UT_11) {
    TempDir dir("mediatool-ut11");
    const auto video = dir.file("clip.mp4");
    const auto srt = dir.file("clip.srt", "1");
    dir.file("clip.ass", "1");
    const auto found = findSidecar(video, {".srt", ".ass", ".vtt"});
    EXPECT(found.has_value());
    EXPECT(fileNameOf(*found) == "clip.srt");
    EXPECT(*found == srt);
    PlanRequest request;
    request.feature = Feature::MuxSubtitle;
    request.media = clip(video);
    request.externalPath = *found;
    request.outputPath = dir.file("unused.mkv");
    std::filesystem::remove(utf8Path(request.outputPath));
    const auto planned = planJob(request);
    EXPECT(planned.plan.has_value());
    EXPECT(planned.plan->successMessage.find("clip.srt") != std::string::npos);
}

TEST(UT_12) {
    TempDir dir("mediatool-ut12");
    const auto video = dir.file("clip.mp4");
    const auto flac = dir.file("clip.flac");
    const auto found = findSidecar(video, {".m4a", ".mp3", ".wav", ".flac"});
    EXPECT(found.has_value());
    EXPECT(*found == flac);
}

TEST(UT_13) {
    TempDir dir("mediatool-ut13");
    const auto video = dir.file("clip.mp4");
    EXPECT(!findSidecar(video, {".srt", ".ass", ".vtt"}));
    EXPECT(!findSidecar(video, {".m4a", ".mp3", ".wav", ".flac"}));
    EXPECT(std::string(kNoSidecarSub) == "找不到同名字幕");
    EXPECT(std::string(kNoSidecarAudio) == "找不到同名音频");
}

TEST(UT_14) { EXPECT(containerAccepts(ContainerKind::Mp4, {stream("video", "h264"), stream("audio", "aac")})); }

TEST(UT_15) { EXPECT(!containerAccepts(ContainerKind::Webm, {stream("video", "h264"), stream("audio", "aac")})); }

TEST(UT_16) { EXPECT(containerAccepts(ContainerKind::Webm, {stream("video", "vp9"), stream("audio", "opus")})); }

TEST(UT_17) {
    EXPECT(!containerAccepts(ContainerKind::Mp4, {stream("video", "h264"), stream("audio", "aac"), stream("subtitle", "subrip")}));
}

TEST(UT_18) {
    EXPECT(containerAccepts(ContainerKind::Mp4, {stream("video", "h264"), stream("audio", "aac"), stream("attachment", "mjpeg"), stream("data", "bin")}));
}

TEST(UT_19) {
    const auto scale = compressScale(1920, 1080, ResolutionPreset::P720);
    EXPECT(scale.has_value());
    EXPECT(*scale == "scale=-2:720");
}

TEST(UT_20) { EXPECT(!compressScale(640, 480, ResolutionPreset::P1080)); }

TEST(UT_21) {
    const auto scale = compressScale(1919, 1080, ResolutionPreset::Original);
    EXPECT(scale.has_value());
    EXPECT(*scale == "scale=trunc(iw/2)*2:trunc(ih/2)*2");
}

TEST(UT_22) {
    const auto [width, height] = evenSize(1919, 1080);
    EXPECT(width == 1918);
    EXPECT(height == 1080);
}

TEST(UT_23) {
    MediaInfo info;
    info.durationSeconds = 90;
    info.width = 1920;
    info.height = 1080;
    info.frameNumerator = 24;
    info.frameDenominator = 1;
    info.videoCodec = "h264";
    info.videoBitrateBits = 2500000;
    AudioTrack audio;
    audio.number = 1;
    audio.codec = "aac";
    audio.channels = 2;
    info.audios.push_back(audio);
    SubtitleTrack subtitle;
    subtitle.number = 1;
    subtitle.codec = "subrip";
    subtitle.languageTag = "chi";
    subtitle.kind = SubtitleKind::Text;
    info.subtitles.push_back(subtitle);
    ChapterMark chapter;
    chapter.startSeconds = 10;
    chapter.title = "开场";
    info.chapters.push_back(chapter);
    const std::string expected =
        "时长：00:01:30.000\n"
        "分辨率：1920×1080\n"
        "帧率：24\n"
        "视频编码：h264\n"
        "视频码率：2500 kbps\n"
        "音轨：\n"
        "1. aac，2 声道，未标记\n"
        "字幕：\n"
        "1. subrip，chi，文本\n"
        "章节：\n"
        "00:00:10.000 开场\n";
    EXPECT(formatInfo(info) == expected);
}

TEST(UT_24) {
    EXPECT(!volumeFilter(100));
    const auto half = volumeFilter(50);
    const auto mute = volumeFilter(0);
    EXPECT(half.has_value() && *half == "volume=0.50");
    EXPECT(mute.has_value() && *mute == "volume=0.00");
}

TEST(UT_25) {
    PlanRequest request;
    request.feature = Feature::ExtractAudio;
    request.media = clip("C:/video/clip.mp4");
    AudioTrack first;
    first.number = 1;
    first.codec = "aac";
    first.channels = 2;
    AudioTrack second = first;
    second.number = 2;
    request.media.audios = {first, second};
    request.trackNumber = 2;
    request.audioFormat = AudioFormat::Mp3;
    request.volume = 100;
    request.outputPath = "C:/video/clip_audio.mp3";
    const auto planned = planJob(request);
    EXPECT(planned.plan.has_value());
    const auto& args = planned.plan->steps.front().arguments;
    EXPECT(hasToken(args, "0:a:1"));
    EXPECT(hasToken(args, "libmp3lame"));
    EXPECT(hasToken(args, "192k"));
    EXPECT(!hasToken(args, "-ar"));
}

TEST(UT_26) {
    PlanRequest request;
    request.feature = Feature::ExtractSubtitle;
    request.media = clip("C:/video/clip.mkv");
    SubtitleTrack subtitle;
    subtitle.number = 1;
    subtitle.codec = "hdmv_pgs_subtitle";
    subtitle.kind = SubtitleKind::Image;
    request.media.subtitles.push_back(subtitle);
    request.subtitleFormat = SubtitleFormat::Srt;
    request.outputPath = "C:/video/clip_subtitle.srt";
    const auto planned = planJob(request);
    EXPECT(!planned.plan);
    EXPECT(planned.failureMessage == kImageSubtitle);
}

TEST(UT_27) {
    PlanRequest request;
    request.feature = Feature::Convert;
    request.convertMode = ConvertMode::Remux;
    request.container = ContainerKind::Webm;
    request.media = clip("C:/video/clip.mp4");
    request.media.fitStreams = {stream("video", "h264"), stream("audio", "aac")};
    request.outputPath = "C:/video/clip_remux.webm";
    const auto planned = planJob(request);
    EXPECT(!planned.plan);
    EXPECT(!planned.plan || planned.plan->steps.empty());
}

TEST(UT_28) {
    TempDir dir("mediatool-ut28");
    PlanRequest request;
    request.feature = Feature::Audio;
    request.audioEdit = AudioEditMode::Replace;
    request.media = clip(dir.file("clip.avi"));
    request.externalPath = dir.file("tone.wav");
    request.outputPath = dir.str() + "/clip_replaced.mp4";
    EXPECT(outputExtensionFor(request) == ".mp4");
    const auto planned = planJob(request);
    EXPECT(planned.plan.has_value());
    EXPECT(planned.plan->successMessage.find(kUsedMp4) != std::string::npos);
}

TEST(UT_29) {
    TempDir dir("mediatool-ut29");
    PlanRequest request;
    request.feature = Feature::Merge;
    request.container = ContainerKind::Mp4;
    request.tempDir = dir.str();
    request.outputPath = dir.str() + "/out.mp4";
    auto silent = clip(dir.file("silent.mp4"));
    auto sound = clip(dir.file("sound.mp4"));
    AudioTrack audio;
    audio.number = 1;
    audio.codec = "aac";
    audio.channels = 2;
    audio.sampleRate = 48000;
    sound.audios.push_back(audio);
    request.mergeInputs = {sound, silent};
    const auto planned = planJob(request);
    EXPECT(planned.plan.has_value());
    bool found = false;
    for (const auto& step : planned.plan->steps) {
        if (hasToken(step.arguments, "anullsrc=channel_layout=stereo:sample_rate=48000") && hasToken(step.arguments, silent.path)) found = true;
    }
    EXPECT(found);
}

TEST(UT_30) {
    TempDir dir("mediatool-ut30");
    PlanRequest request;
    request.feature = Feature::Merge;
    request.container = ContainerKind::Mp4;
    request.tempDir = dir.str();
    request.outputPath = dir.str() + "/out.mp4";
    request.mergeInputs = {clip(dir.file("a.mp4"), "h264"), clip(dir.file("b.mp4"), "hevc")};
    const auto planned = planJob(request);
    EXPECT(planned.plan.has_value());
    int temps = 0;
    int concats = 0;
    for (const auto& step : planned.plan->steps) {
        if (step.outputPath.find("0001.mkv") != std::string::npos || step.outputPath.find("0002.mkv") != std::string::npos) ++temps;
        if (hasToken(step.arguments, "concat")) ++concats;
    }
    EXPECT(temps == 2);
    EXPECT(concats == 1);
}

TEST(UT_31) {
    const auto args = videoEncodeArgs(VideoEncoder::H264, EncoderImpl::Nvenc, std::nullopt);
    EXPECT(hasToken(args, "h264_nvenc"));
    EXPECT(hasToken(args, "p5"));
    EXPECT(hasToken(args, "23"));
    EXPECT(!hasToken(args, "libx264"));
}

TEST(UT_32) {
    EncoderAvailability available;
    available.h264Qsv = true;
    available.h265Qsv = true;
    available.h264Amf = true;
    available.h265Amf = true;
    EXPECT(selectEncoder(VideoEncoder::H264, available) == EncoderImpl::Qsv);
    EXPECT(selectEncoder(VideoEncoder::H265, available) == EncoderImpl::Qsv);
}

TEST(UT_33) {
    const auto args = videoEncodeArgs(VideoEncoder::H265, EncoderImpl::Amf, 1500);
    EXPECT(hasToken(args, "hevc_amf"));
    EXPECT(hasToken(args, "1500k"));
    EXPECT(!hasToken(args, "-cq"));
    EXPECT(!hasToken(args, "-crf"));
}

TEST(UT_34) {
    const auto args = videoEncodeArgs(VideoEncoder::Vp9, EncoderImpl::Cpu, std::nullopt);
    EXPECT(hasToken(args, "libvpx-vp9"));
    EXPECT(hasToken(args, "good"));
    EXPECT(hasToken(args, "2"));
}

TEST(UT_35) {
    PlanRequest request;
    request.feature = Feature::Convert;
    request.convertMode = ConvertMode::Compress;
    request.container = ContainerKind::Mkv;
    request.videoEncoder = VideoEncoder::H264;
    request.media = clip("C:/video/clip.mkv");
    SubtitleTrack text;
    text.number = 1;
    text.codec = "ass";
    text.kind = SubtitleKind::Text;
    SubtitleTrack image;
    image.number = 2;
    image.codec = "hdmv_pgs_subtitle";
    image.kind = SubtitleKind::Image;
    request.media.subtitles = {text, image};
    request.outputPath = "C:/video/clip_compress.mkv";
    const auto planned = planJob(request);
    EXPECT(planned.plan.has_value());
    const auto& args = planned.plan->steps.front().arguments;
    EXPECT(hasToken(args, "0:s?"));
    EXPECT(hasToken(args, "copy"));
    bool sawSubtitleCopy = false;
    for (size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == "-c:s" && args[i + 1] == "copy") sawSubtitleCopy = true;
    }
    EXPECT(sawSubtitleCopy);
}

TEST(UT_36) {
    PlanRequest request;
    request.feature = Feature::Convert;
    request.convertMode = ConvertMode::Compress;
    request.container = ContainerKind::Mp4;
    request.videoEncoder = VideoEncoder::H264;
    request.media = clip("C:/video/clip.mkv");
    SubtitleTrack text;
    text.number = 1;
    text.codec = "subrip";
    text.kind = SubtitleKind::Text;
    SubtitleTrack image;
    image.number = 2;
    image.codec = "hdmv_pgs_subtitle";
    image.kind = SubtitleKind::Image;
    request.media.subtitles = {text, image};
    request.outputPath = "C:/video/clip_compress.mp4";
    const auto planned = planJob(request);
    EXPECT(planned.plan.has_value());
    const auto& args = planned.plan->steps.front().arguments;
    EXPECT(hasToken(args, "0:s:0"));
    EXPECT(!hasToken(args, "0:s:1"));
    EXPECT(!hasToken(args, "0:s?"));
    bool sawMovText = false;
    for (size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == "-c:s" && args[i + 1] == "mov_text") sawMovText = true;
    }
    EXPECT(sawMovText);
}

TEST(UT_37) {
    PlanRequest request;
    request.feature = Feature::Convert;
    request.convertMode = ConvertMode::Compress;
    request.container = ContainerKind::Webm;
    request.videoEncoder = VideoEncoder::Vp9;
    request.media = clip("C:/video/clip.mkv");
    SubtitleTrack text;
    text.number = 1;
    text.codec = "webvtt";
    text.kind = SubtitleKind::Text;
    request.media.subtitles = {text};
    request.outputPath = "C:/video/clip_compress.webm";
    const auto planned = planJob(request);
    EXPECT(planned.plan.has_value());
    const auto& args = planned.plan->steps.front().arguments;
    EXPECT(hasToken(args, "0:s:0"));
    bool sawWebVtt = false;
    for (size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == "-c:s" && args[i + 1] == "webvtt") sawWebVtt = true;
    }
    EXPECT(sawWebVtt);
}
