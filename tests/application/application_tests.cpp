#include "test_harness.hpp"

#include "mediatool/logic.hpp"
#include "mediatool/messages.hpp"
#include "mediatool/task_runner.hpp"

#include <filesystem>
#include <fstream>
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
    std::string touch(const std::string& name) const {
        const auto full = path / std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(name.data()), name.size()));
        std::ofstream stream(full, std::ios::binary);
        stream << "x";
        return toUtf8(full);
    }
};

MediaInfo video(const std::string& path, const std::string& codec = "h264", bool withAudio = true) {
    MediaInfo info;
    info.path = path;
    info.durationSeconds = 1;
    info.width = 320;
    info.height = 240;
    info.frameNumerator = 30;
    info.frameDenominator = 1;
    info.videoCodec = codec;
    info.pixelFormat = "yuv420p";
    if (withAudio) {
        AudioTrack audio;
        audio.number = 1;
        audio.codec = "aac";
        audio.channels = 2;
        audio.sampleRate = 48000;
        info.audios.push_back(audio);
    }
    FitStream picture;
    picture.codecType = "video";
    picture.codecName = codec;
    info.fitStreams.push_back(picture);
    return info;
}

void writeOutput(const std::vector<std::string>& args) {
    if (args.empty()) return;
    std::ofstream stream(utf8Path(args.back()), std::ios::binary);
    stream << "ok";
}
}  // namespace

TEST(AT_01) {
    TempDir dir("mediatool-at01");
    const auto first = dir.touch("a.mp4");
    const auto second = dir.touch("b.mp4");
    const auto third = dir.touch("c.mp4");
    int calls = 0;
    CancelState cancel;
    TaskRunner runner;
    runner.cancel = &cancel;
    runner.probe = [](const std::string& path, std::string&) { return std::optional<MediaInfo>(video(path, "h264", path.find("b.mp4") == std::string::npos)); };
    runner.run = [&](const std::vector<std::string>& args, double, const std::function<void(int)>&, const std::function<bool()>&) {
        ++calls;
        writeOutput(args);
        return 0;
    };
    JobRequest job;
    job.folderPath = dir.str();
    job.options.feature = Feature::ExtractAudio;
    job.options.trackNumber = 1;
    job.options.audioFormat = AudioFormat::Mp3;
    const auto results = runner.execute(job);
    EXPECT(results.size() == 3);
    EXPECT(results[0].status == ItemStatus::Succeeded);
    EXPECT(results[1].status == ItemStatus::Failed);
    EXPECT(results[1].message == kNoAudio);
    EXPECT(results[2].status == ItemStatus::Succeeded);
    EXPECT(calls == 2);
    EXPECT(fileNameOf(first).size() > 0 && fileNameOf(second).size() > 0 && fileNameOf(third).size() > 0);
}

TEST(AT_02) {
    TempDir dir("mediatool-at02");
    dir.touch("a.mp4");
    dir.touch("b.mp4");
    dir.touch("c.mp4");
    CancelState cancel;
    TaskRunner runner;
    runner.cancel = &cancel;
    runner.probe = [](const std::string& path, std::string&) { return std::optional<MediaInfo>(video(path)); };
    runner.run = [&](const std::vector<std::string>& args, double, const std::function<void(int)>&, const std::function<bool()>&) {
        if (hasToken(args, dir.touch("b.mp4"))) cancel.stop.store(true);
        for (const auto& arg : args) {
            if (arg.find("b.mp4") != std::string::npos) cancel.stop.store(true);
        }
        if (cancel.stop.load() && [&] {
                for (const auto& arg : args) {
                    if (arg.find("b.mp4") != std::string::npos) return true;
                }
                return false;
            }()) {
            return 2;
        }
        writeOutput(args);
        return 0;
    };
    JobRequest job;
    job.folderPath = dir.str();
    job.options.feature = Feature::ExtractAudio;
    job.options.audioFormat = AudioFormat::Wav;
    const auto results = runner.execute(job);
    EXPECT(results.size() == 3);
    EXPECT(results[0].status == ItemStatus::Succeeded);
    EXPECT(results[1].status == ItemStatus::Canceled);
    EXPECT(results[2].status == ItemStatus::NotProcessed);
}

TEST(AT_03) {
    TempDir dir("mediatool-at03");
    dir.touch("a.mp4");
    dir.touch("b.mp4");
    dir.touch("c.mp4");
    CancelState cancel;
    TaskRunner runner;
    runner.cancel = &cancel;
    runner.probe = [](const std::string& path, std::string&) { return std::optional<MediaInfo>(video(path)); };
    runner.run = [&](const std::vector<std::string>& args, double, const std::function<void(int)>&, const std::function<bool()>&) {
        bool current = false;
        for (const auto& arg : args) {
            if (arg.find("b.mp4") != std::string::npos) current = true;
        }
        if (current) {
            cancel.skip.store(true);
            return 2;
        }
        writeOutput(args);
        return 0;
    };
    JobRequest job;
    job.folderPath = dir.str();
    job.options.feature = Feature::ExtractAudio;
    job.options.audioFormat = AudioFormat::Wav;
    const auto results = runner.execute(job);
    EXPECT(results.size() == 3);
    EXPECT(results[1].status == ItemStatus::Canceled);
    EXPECT(results[2].status == ItemStatus::Succeeded);
}

TEST(AT_04) {
    TempDir dir("mediatool-at04");
    const auto first = dir.touch("a.mp4");
    const auto second = dir.touch("b.mp4");
    int calls = 0;
    TaskRunner runner;
    runner.probe = [&](const std::string& path, std::string&) -> std::optional<MediaInfo> {
        if (path == second) return std::nullopt;
        return video(path);
    };
    runner.run = [&](const std::vector<std::string>&, double, const std::function<void(int)>&, const std::function<bool()>&) {
        ++calls;
        return 0;
    };
    JobRequest job;
    job.files = {first, second};
    job.options.feature = Feature::Merge;
    job.options.container = ContainerKind::Mp4;
    const auto results = runner.execute(job);
    EXPECT(calls == 0);
    EXPECT(results.size() == 1);
    EXPECT(results[0].message.find(fileNameOf(second)) != std::string::npos);
}

TEST(AT_05) {
    TempDir dir("mediatool-at05");
    const auto file = dir.touch("only.mp4");
    int asks = 0;
    CancelState cancel;
    TaskRunner runner;
    runner.cancel = &cancel;
    runner.probe = [](const std::string& path, std::string&) { return std::optional<MediaInfo>(video(path)); };
    runner.run = [&](const std::vector<std::string>&, double, const std::function<void(int)>&, const std::function<bool()>&) {
        cancel.stop.store(true);
        return 2;
    };
    JobRequest job;
    job.files = {file};
    job.options.feature = Feature::ExtractAudio;
    job.options.audioFormat = AudioFormat::Wav;
    const auto results = runner.execute(job);
    EXPECT(asks == 0);
    EXPECT(results.size() == 1);
    EXPECT(results[0].status == ItemStatus::Canceled);
}

TEST(AT_06) {
    TempDir dir("mediatool-at06");
    const auto file = dir.touch("clip.mp4");
    std::vector<std::vector<std::string>> calls;
    TaskRunner runner;
    runner.probe = [](const std::string& path, std::string&) { return std::optional<MediaInfo>(video(path)); };
    runner.run = [&](const std::vector<std::string>& args, double, const std::function<void(int)>&, const std::function<bool()>&) {
        calls.push_back(args);
        if (calls.size() == 1) return 1;
        writeOutput(args);
        return 0;
    };
    JobRequest job;
    job.files = {file};
    job.options.feature = Feature::Convert;
    job.options.convertMode = ConvertMode::Compress;
    job.options.container = ContainerKind::Mp4;
    job.options.videoEncoder = VideoEncoder::H264;
    job.options.encoderImpl = EncoderImpl::Nvenc;
    const auto results = runner.execute(job);
    EXPECT(results.size() == 1);
    EXPECT(results[0].status == ItemStatus::Succeeded);
    EXPECT(calls.size() == 2);
    EXPECT(hasToken(calls[1], "libx264"));
}

TEST(AT_07) {
    TempDir dir("mediatool-at07");
    const auto first = dir.touch("a.mp4");
    const auto second = dir.touch("b.mp4");
    std::string nvencTemp;
    bool tempRemovedBeforeCpu = false;
    int cpuCalls = 0;
    TaskRunner runner;
    runner.probe = [](const std::string& path, std::string&) {
        const auto codec = path.find("b.mp4") == std::string::npos ? "h264" : "hevc";
        return std::optional<MediaInfo>(video(path, codec));
    };
    runner.run = [&](const std::vector<std::string>& args, double, const std::function<void(int)>&, const std::function<bool()>&) {
        if (hasToken(args, "libx264")) {
            if (cpuCalls == 0) tempRemovedBeforeCpu = !nvencTemp.empty() && !std::filesystem::exists(utf8Path(nvencTemp));
            ++cpuCalls;
            return 1;
        }
        static int nvencCalls = 0;
        ++nvencCalls;
        if (nvencCalls == 1) {
            writeOutput(args);
            nvencTemp = args.back();
            return 0;
        }
        return 1;
    };
    JobRequest job;
    job.files = {first, second};
    job.options.feature = Feature::Merge;
    job.options.container = ContainerKind::Mp4;
    job.options.encoderImpl = EncoderImpl::Nvenc;
    const auto results = runner.execute(job);
    EXPECT(tempRemovedBeforeCpu);
    EXPECT(cpuCalls >= 1);
    EXPECT(results.size() == 1);
    EXPECT(results[0].status == ItemStatus::Failed);
    EXPECT(results[0].outputPath.empty());
    if (!nvencTemp.empty()) EXPECT(!std::filesystem::exists(utf8Path(nvencTemp)));
}
