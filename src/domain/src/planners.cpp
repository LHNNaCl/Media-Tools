#include "mediatool/logic.hpp"
#include "mediatool/messages.hpp"

#include <algorithm>
#include <sstream>

namespace MediaTool {
namespace {

std::string fixedSeconds(double value) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out.setf(std::ios::fixed);
    out.precision(3);
    out << value;
    return out.str();
}

std::string frameRateArg(const MediaInfo& info) {
    if (!info.frameNumerator || !info.frameDenominator || *info.frameDenominator == 0) return "30";
    return std::to_string(*info.frameNumerator) + "/" + std::to_string(*info.frameDenominator);
}

void addVideoEncode(std::vector<std::string>& args, VideoEncoder family, EncoderImpl impl, std::optional<int> bitrate,
                    const std::string& outputPath) {
    auto encoded = videoEncodeArgs(family, impl, bitrate);
    args.insert(args.end(), encoded.begin(), encoded.end());
    args.insert(args.end(), {"-pix_fmt", "yuv420p"});
    if (family == VideoEncoder::H265) {
        const auto ext = extensionOf(outputPath);
        if (ext == ".mp4" || ext == ".mov") args.insert(args.end(), {"-tag:v", "hvc1"});
    }
}

struct AudioChoice {
    std::string containerExtension;
    std::string codec;
    std::string bitrate;
    bool changedToMp4 = false;
};

AudioChoice audioChoice(const std::string& sourcePath, bool reencode) {
    const auto ext = extensionOf(sourcePath);
    AudioChoice choice;
    choice.containerExtension = ext.empty() ? ".mp4" : ext;
    if (!reencode) {
        choice.codec = "copy";
        return choice;
    }
    if (ext == ".webm") {
        choice.codec = "libopus";
        choice.bitrate = "128k";
        return choice;
    }
    choice.codec = "aac";
    choice.bitrate = "160k";
    if (ext == ".avi") {
        choice.containerExtension = ".mp4";
        choice.changedToMp4 = true;
    }
    return choice;
}

PlanResult fail(const std::string& message) {
    PlanResult result;
    result.failureMessage = message;
    return result;
}

PlanResult ok(ExecutionPlan plan) {
    PlanResult result;
    result.plan = std::move(plan);
    return result;
}

const AudioTrack* audioAt(const MediaInfo& info, int number) {
    for (const auto& track : info.audios) {
        if (track.number == number) return &track;
    }
    return nullptr;
}

const SubtitleTrack* subtitleAt(const MediaInfo& info, int number) {
    for (const auto& track : info.subtitles) {
        if (track.number == number) return &track;
    }
    return nullptr;
}

PlanResult planInfo(const PlanRequest& request) {
    ExecutionPlan plan;
    plan.successMessage = kDone;
    ProcessStep step;
    step.kind = StepKind::WriteText;
    step.outputPath = request.outputPath;
    step.textContent = formatInfo(request.media);
    plan.steps.push_back(std::move(step));
    return ok(std::move(plan));
}

PlanResult planExtractAudio(const PlanRequest& request) {
    const auto* track = audioAt(request.media, request.trackNumber);
    if (!track) return fail(kNoAudio);
    ExecutionPlan plan;
    plan.successMessage = kDone;
    auto args = ffmpegPrefix();
    args.insert(args.end(), {"-i", request.media.path, "-map", "0:a:" + std::to_string(track->number - 1), "-vn"});
    if (auto gain = volumeFilter(request.volume)) args.insert(args.end(), {"-filter:a", *gain});
    switch (request.audioFormat) {
    case AudioFormat::Mp3: args.insert(args.end(), {"-c:a", "libmp3lame", "-b:a", "192k"}); break;
    case AudioFormat::M4a: args.insert(args.end(), {"-c:a", "aac", "-b:a", "192k"}); break;
    case AudioFormat::Wav: args.insert(args.end(), {"-c:a", "pcm_s16le"}); break;
    case AudioFormat::Flac: args.insert(args.end(), {"-c:a", "flac"}); break;
    }
    appendFaststartIfNeeded(args, request.outputPath);
    args.push_back(request.outputPath);
    plan.steps.push_back({StepKind::Ffmpeg, std::move(args), request.outputPath, std::nullopt});
    return ok(std::move(plan));
}

PlanResult planExtractSubtitle(const PlanRequest& request) {
    const auto* track = subtitleAt(request.media, request.trackNumber);
    if (!track) return fail(kNoSubtitle);
    if (track->kind == SubtitleKind::Image && request.subtitleFormat != SubtitleFormat::Raw) return fail(kImageSubtitle);
    ExecutionPlan plan;
    plan.successMessage = kDone;
    auto args = ffmpegPrefix();
    args.insert(args.end(), {"-i", request.media.path, "-map", "0:s:" + std::to_string(track->number - 1)});
    const char* codec = "copy";
    switch (request.subtitleFormat) {
    case SubtitleFormat::Srt: codec = "srt"; break;
    case SubtitleFormat::Ass: codec = "ass"; break;
    case SubtitleFormat::Vtt: codec = "webvtt"; break;
    case SubtitleFormat::Raw: codec = "copy"; break;
    }
    args.insert(args.end(), {"-c:s", codec, request.outputPath});
    plan.steps.push_back({StepKind::Ffmpeg, std::move(args), request.outputPath, std::nullopt});
    return ok(std::move(plan));
}

PlanResult planMux(const PlanRequest& request) {
    if (request.externalPath.empty()) return fail(kNoSidecarSub);
    if (!fileNonEmpty(request.externalPath)) return fail(kBadSubtitle);
    const auto ext = extensionOf(request.externalPath);
    const char* codec = "srt";
    if (ext == ".ass") codec = "ass";
    else if (ext == ".vtt") codec = "webvtt";
    ExecutionPlan plan;
    plan.successMessage = successWith(usedFile(fileNameOf(request.externalPath)));
    auto args = ffmpegPrefix();
    args.insert(args.end(), {"-i", request.media.path, "-i", request.externalPath, "-map", "0:v:0", "-map", "0:a?",
                             "-map", "1:0", "-c:v", "copy", "-c:a", "copy", "-c:s", codec, request.outputPath});
    plan.steps.push_back({StepKind::Ffmpeg, std::move(args), request.outputPath, std::nullopt});
    return ok(std::move(plan));
}

PlanResult planTrim(const PlanRequest& request) {
    if (!request.media.durationSeconds) return fail(kCannotRead);
    const auto other = request.trimMode == TrimMode::StartDuration ? request.trimDuration : request.trimEnd;
    std::string error;
    const auto range = resolveTrim(request.trimMode, request.trimStart, other, request.media.durationSeconds, error);
    if (!range) return fail(error);
    const bool webm = extensionOf(request.media.path) == ".webm";
    const bool hasAudio = !request.media.audios.empty();
    ExecutionPlan plan;
    plan.usesHardware = request.encoderImpl != EncoderImpl::Cpu;
    plan.successMessage = kDone;
    auto args = ffmpegPrefix();
    args.insert(args.end(), {"-i", request.media.path, "-ss", fixedSeconds(range->start), "-t", fixedSeconds(range->duration),
                             "-map", "0:v:0"});
    if (hasAudio) args.insert(args.end(), {"-map", "0:a?"});
    else args.push_back("-an");
    if (auto scale = compressScale(request.media.width, request.media.height, ResolutionPreset::Original)) {
        args.insert(args.end(), {"-vf", *scale});
    }
    addVideoEncode(args, webm ? VideoEncoder::Vp9 : VideoEncoder::H264, request.encoderImpl, std::nullopt, request.outputPath);
    if (hasAudio) {
        if (webm) args.insert(args.end(), {"-c:a", "libopus", "-b:a", "128k"});
        else args.insert(args.end(), {"-c:a", "aac", "-b:a", "160k"});
    }
    appendFaststartIfNeeded(args, request.outputPath);
    args.push_back(request.outputPath);
    ProcessStep step{StepKind::Ffmpeg, std::move(args), request.outputPath, std::nullopt, range->duration};
    plan.steps.push_back(std::move(step));
    return ok(std::move(plan));
}

std::optional<std::string> mapCompressSubtitles(std::vector<std::string>& args, const MediaInfo& media,
                                                 ContainerKind container) {
    if (media.subtitles.empty()) return std::nullopt;
    if (container == ContainerKind::Mkv) {
        args.insert(args.end(), {"-map", "0:s?"});
        return "copy";
    }
    bool mapped = false;
    for (const auto& track : media.subtitles) {
        if (track.kind != SubtitleKind::Text) continue;
        args.insert(args.end(), {"-map", "0:s:" + std::to_string(track.number - 1)});
        mapped = true;
    }
    if (!mapped) return std::nullopt;
    return container == ContainerKind::Webm ? "webvtt" : "mov_text";
}

PlanResult planConvert(const PlanRequest& request) {
    if (request.convertMode == ConvertMode::Remux) {
        if (!containerAccepts(request.container, request.media.fitStreams)) return fail(kContainerFit);
        ExecutionPlan plan;
        plan.successMessage = kDone;
        auto args = ffmpegPrefix();
        args.insert(args.end(), {"-i", request.media.path});
        if (request.media.fitStreams.empty()) args.insert(args.end(), {"-map", "0"});
        for (const auto& stream : request.media.fitStreams) {
            args.insert(args.end(), {"-map", "0:" + std::to_string(stream.absoluteIndex)});
        }
        args.insert(args.end(), {"-c", "copy"});
        appendFaststartIfNeeded(args, request.outputPath);
        args.push_back(request.outputPath);
        plan.steps.push_back({StepKind::Ffmpeg, std::move(args), request.outputPath, std::nullopt});
        return ok(std::move(plan));
    }
    if (request.container == ContainerKind::Webm && request.videoEncoder != VideoEncoder::Vp9) return fail(kWebmVp9Only);
    const auto family = request.container == ContainerKind::Webm ? VideoEncoder::Vp9 : request.videoEncoder;
    ExecutionPlan plan;
    plan.usesHardware = request.encoderImpl != EncoderImpl::Cpu;
    plan.successMessage = kDone;
    auto args = ffmpegPrefix();
    args.insert(args.end(), {"-i", request.media.path, "-map", "0:v:0"});
    const bool hasAudio = !request.media.audios.empty();
    if (hasAudio) args.insert(args.end(), {"-map", "0:a?"});
    else args.push_back("-an");
    const auto subtitleCodec = mapCompressSubtitles(args, request.media, request.container);
    if (auto scale = compressScale(request.media.width, request.media.height, request.resolution)) {
        args.insert(args.end(), {"-vf", *scale});
    }
    addVideoEncode(args, family, request.encoderImpl, request.bitrateKbps, request.outputPath);
    if (hasAudio) {
        if (request.container == ContainerKind::Webm) args.insert(args.end(), {"-c:a", "libopus", "-b:a", "128k"});
        else args.insert(args.end(), {"-c:a", "aac", "-b:a", "160k"});
    }
    if (subtitleCodec) args.insert(args.end(), {"-c:s", *subtitleCodec});
    appendFaststartIfNeeded(args, request.outputPath);
    args.push_back(request.outputPath);
    plan.steps.push_back({StepKind::Ffmpeg, std::move(args), request.outputPath, std::nullopt});
    return ok(std::move(plan));
}

PlanResult planAudioEdit(const PlanRequest& request) {
    if (request.audioEdit != AudioEditMode::Replace && request.media.audios.empty()) return fail(kNoAudio);
    if (request.audioEdit == AudioEditMode::Keep && !audioAt(request.media, request.trackNumber)) return fail(kNoAudio);
    if (request.audioEdit == AudioEditMode::Replace) {
        if (request.externalPath.empty()) return fail(kNoSidecarAudio);
        if (!request.media.durationSeconds) return fail(kCannotRead);
    }
    bool reencode = request.audioEdit != AudioEditMode::Remove;
    if (request.audioEdit == AudioEditMode::Keep) {
        const auto* track = audioAt(request.media, request.trackNumber);
        const auto ext = extensionOf(request.media.path);
        bool canCopy = false;
        if (ext == ".mkv") canCopy = true;
        else if (ext == ".webm") canCopy = containerAcceptsAudio(ContainerKind::Webm, track->codec);
        else if (ext == ".avi") canCopy = lowerCopy(track->codec) == "mp3";
        else canCopy = containerAcceptsAudio(ContainerKind::Mp4, track->codec);
        reencode = request.volume != 100 || !canCopy;
    }
    if (request.audioEdit == AudioEditMode::Replace) reencode = true;
    auto choice = audioChoice(request.media.path, reencode && request.audioEdit != AudioEditMode::Remove);
    ExecutionPlan plan;
    plan.successMessage = choice.changedToMp4 ? successWith(kUsedMp4) : kDone;
    auto args = ffmpegPrefix();
    if (request.audioEdit == AudioEditMode::Remove) {
        args.insert(args.end(), {"-i", request.media.path, "-map", "0:v:0", "-c:v", "copy", "-an", request.outputPath});
        plan.steps.push_back({StepKind::Ffmpeg, std::move(args), request.outputPath, std::nullopt});
        return ok(std::move(plan));
    }
    if (request.audioEdit == AudioEditMode::Keep) {
        const auto* track = audioAt(request.media, request.trackNumber);
        args.insert(args.end(), {"-i", request.media.path, "-map", "0:v:0", "-map", "0:a:" + std::to_string(track->number - 1),
                                 "-c:v", "copy"});
        if (!reencode) args.insert(args.end(), {"-c:a", "copy"});
        else {
            if (auto gain = volumeFilter(request.volume)) args.insert(args.end(), {"-filter:a", *gain});
            args.insert(args.end(), {"-c:a", choice.codec, "-b:a", choice.bitrate});
        }
        appendFaststartIfNeeded(args, request.outputPath);
        args.push_back(request.outputPath);
        plan.steps.push_back({StepKind::Ffmpeg, std::move(args), request.outputPath, std::nullopt});
        return ok(std::move(plan));
    }
    const std::string duration = fixedSeconds(*request.media.durationSeconds);
    std::string filter = "atrim=end=" + duration + ",apad=whole_dur=" + duration;
    if (auto gain = volumeFilter(request.volume)) filter = *gain + "," + filter;
    args.insert(args.end(), {"-i", request.media.path, "-i", request.externalPath, "-map", "0:v:0", "-map", "1:a:0", "-c:v",
                             "copy", "-filter:a", filter, "-c:a", choice.codec, "-b:a", choice.bitrate});
    appendFaststartIfNeeded(args, request.outputPath);
    args.push_back(request.outputPath);
    plan.steps.push_back({StepKind::Ffmpeg, std::move(args), request.outputPath, std::nullopt});
    return ok(std::move(plan));
}

bool sameTopology(const MediaInfo& left, const MediaInfo& right) {
    if (left.videoCodec != right.videoCodec || left.width != right.width || left.height != right.height ||
        left.pixelFormat != right.pixelFormat || left.frameNumerator != right.frameNumerator ||
        left.frameDenominator != right.frameDenominator || left.audios.size() != right.audios.size() ||
        left.subtitles.size() != right.subtitles.size()) {
        return false;
    }
    for (size_t i = 0; i < left.audios.size(); ++i) {
        if (left.audios[i].codec != right.audios[i].codec || left.audios[i].sampleRate != right.audios[i].sampleRate ||
            left.audios[i].channels != right.audios[i].channels) {
            return false;
        }
    }
    for (size_t i = 0; i < left.subtitles.size(); ++i) {
        if (left.subtitles[i].codec != right.subtitles[i].codec) return false;
    }
    return true;
}

PlanResult planMerge(const PlanRequest& request) {
    if (request.mergeInputs.size() < 2) return fail(kNeedTwo);
    for (const auto& input : request.mergeInputs) {
        if (input.videoCodec.empty() || !input.frameNumerator || !input.frameDenominator) {
            return fail(cannotReadNamed(fileNameOf(input.path)));
        }
    }
    bool direct = true;
    for (size_t i = 1; i < request.mergeInputs.size(); ++i) {
        if (!sameTopology(request.mergeInputs.front(), request.mergeInputs[i])) direct = false;
    }
    std::vector<FitStream> common = request.mergeInputs.front().fitStreams;
    if (!containerAccepts(request.container, common)) direct = false;
    ExecutionPlan plan;
    plan.successMessage = kDone;
    const auto listPath = toUtf8(utf8Path(request.tempDir) / "list.txt");
    if (direct) {
        std::ostringstream list;
        list.imbue(std::locale::classic());
        for (const auto& input : request.mergeInputs) {
            std::string path = input.path;
            std::string escaped;
            for (char ch : path) {
                if (ch == '\'') escaped += "'\\''";
                else escaped += ch;
            }
            list << "file '" << escaped << "'\n";
        }
        plan.steps.push_back({StepKind::WriteText, {}, listPath, list.str()});
        auto args = ffmpegPrefix();
        args.insert(args.end(), {"-f", "concat", "-safe", "0", "-i", listPath, "-c", "copy"});
        appendFaststartIfNeeded(args, request.outputPath);
        args.push_back(request.outputPath);
        plan.steps.push_back({StepKind::Ffmpeg, std::move(args), request.outputPath, std::nullopt});
        return ok(std::move(plan));
    }
    const bool anyAudio = std::any_of(request.mergeInputs.begin(), request.mergeInputs.end(),
                                      [](const MediaInfo& info) { return !info.audios.empty(); });
    const auto [width, height] = evenSize(request.mergeInputs.front().width, request.mergeInputs.front().height);
    const auto family = request.container == ContainerKind::Webm ? VideoEncoder::Vp9 : VideoEncoder::H264;
    const std::string tempExt = request.container == ContainerKind::Webm ? ".webm" : ".mkv";
    plan.usesHardware = request.encoderImpl != EncoderImpl::Cpu;
    int index = 1;
    for (const auto& input : request.mergeInputs) {
        const std::string tempName = (index < 10 ? "000" : index < 100 ? "00" : index < 1000 ? "0" : "") + std::to_string(index) + tempExt;
        const auto tempPath = toUtf8(utf8Path(request.tempDir) / tempName);
        auto args = ffmpegPrefix();
        const bool silent = anyAudio && input.audios.empty();
        if (silent) {
            if (!input.durationSeconds) return fail(cannotReadNamed(fileNameOf(input.path)));
            args.insert(args.end(), {"-i", input.path, "-f", "lavfi", "-t", fixedSeconds(*input.durationSeconds), "-i",
                                     "anullsrc=channel_layout=stereo:sample_rate=48000", "-map", "0:v:0", "-map", "1:a:0"});
        } else {
            args.insert(args.end(), {"-i", input.path, "-map", "0:v:0"});
            if (anyAudio) args.insert(args.end(), {"-map", "0:a?"});
            else args.push_back("-an");
        }
        args.insert(args.end(), {"-vf", "scale=" + std::to_string(width) + ":" + std::to_string(height), "-r",
                                 frameRateArg(request.mergeInputs.front())});
        addVideoEncode(args, family, request.encoderImpl, request.bitrateKbps, tempPath);
        if (anyAudio) {
            if (request.container == ContainerKind::Webm) args.insert(args.end(), {"-c:a", "libopus", "-b:a", "128k"});
            else args.insert(args.end(), {"-c:a", "aac", "-b:a", "160k"});
            args.insert(args.end(), {"-ar", "48000", "-ac", "2"});
        }
        args.push_back(tempPath);
        plan.steps.push_back({StepKind::Ffmpeg, std::move(args), tempPath, std::nullopt, input.durationSeconds.value_or(0)});
        ++index;
    }
    std::ostringstream list;
    index = 1;
    for (size_t i = 0; i < request.mergeInputs.size(); ++i, ++index) {
        const std::string tempName = (index < 10 ? "000" : index < 100 ? "00" : index < 1000 ? "0" : "") + std::to_string(index) + tempExt;
        list << "file '" << toUtf8(utf8Path(request.tempDir) / tempName) << "'\n";
    }
    plan.steps.push_back({StepKind::WriteText, {}, listPath, list.str()});
    auto concat = ffmpegPrefix();
    concat.insert(concat.end(), {"-f", "concat", "-safe", "0", "-i", listPath, "-c", "copy"});
    appendFaststartIfNeeded(concat, request.outputPath);
    concat.push_back(request.outputPath);
    plan.steps.push_back({StepKind::Ffmpeg, std::move(concat), request.outputPath, std::nullopt});
    return ok(std::move(plan));
}

}  // namespace

std::string outputSuffix(Feature feature, ConvertMode convertMode, AudioEditMode audioEdit) {
    switch (feature) {
    case Feature::Info: return "_info";
    case Feature::ExtractAudio: return "_audio";
    case Feature::ExtractSubtitle: return "_subtitle";
    case Feature::MuxSubtitle: return "_muxed";
    case Feature::Trim: return "_trim";
    case Feature::Merge: return "_merge";
    case Feature::Convert: return convertMode == ConvertMode::Remux ? "_remux" : "_compress";
    case Feature::Audio:
        if (audioEdit == AudioEditMode::Keep) return "_audiotrack";
        if (audioEdit == AudioEditMode::Replace) return "_replaced";
        return "_silent";
    }
    return "_out";
}

std::string outputExtensionFor(const PlanRequest& request) {
    switch (request.feature) {
    case Feature::Info: return ".txt";
    case Feature::ExtractAudio:
        switch (request.audioFormat) {
        case AudioFormat::Mp3: return ".mp3";
        case AudioFormat::M4a: return ".m4a";
        case AudioFormat::Wav: return ".wav";
        case AudioFormat::Flac: return ".flac";
        }
        break;
    case Feature::ExtractSubtitle:
        switch (request.subtitleFormat) {
        case SubtitleFormat::Srt: return ".srt";
        case SubtitleFormat::Ass: return ".ass";
        case SubtitleFormat::Vtt: return ".vtt";
        case SubtitleFormat::Raw: return ".mkv";
        }
        break;
    case Feature::MuxSubtitle: return ".mkv";
    case Feature::Trim: return extensionOf(request.media.path);
    case Feature::Merge:
    case Feature::Convert: return containerExtension(request.container);
    case Feature::Audio: {
        const auto ext = extensionOf(request.media.path);
        if (request.audioEdit == AudioEditMode::Remove) return ext.empty() ? ".mp4" : ext;
        bool reencode = request.audioEdit == AudioEditMode::Replace || request.volume != 100;
        if (request.audioEdit == AudioEditMode::Keep) {
            const AudioTrack* track = nullptr;
            for (const auto& item : request.media.audios) {
                if (item.number == request.trackNumber) {
                    track = &item;
                    break;
                }
            }
            if (track) {
                bool canCopy = ext == ".mkv" || (ext == ".webm" && containerAcceptsAudio(ContainerKind::Webm, track->codec)) ||
                               (ext == ".avi" && lowerCopy(track->codec) == "mp3") ||
                               (ext != ".webm" && ext != ".avi" && ext != ".mkv" && containerAcceptsAudio(ContainerKind::Mp4, track->codec));
                reencode = request.volume != 100 || !canCopy;
            }
        }
        if (!reencode) return ext.empty() ? ".mp4" : ext;
        if (ext == ".webm") return ".webm";
        if (ext == ".avi") return ".mp4";
        return ext.empty() ? ".mp4" : ext;
    }
    }
    return ".mp4";
}

PlanResult planJob(const PlanRequest& request) {
    PlanResult result = fail(kCorrupt);
    switch (request.feature) {
    case Feature::Info: result = planInfo(request); break;
    case Feature::ExtractAudio: result = planExtractAudio(request); break;
    case Feature::ExtractSubtitle: result = planExtractSubtitle(request); break;
    case Feature::MuxSubtitle: result = planMux(request); break;
    case Feature::Trim: result = planTrim(request); break;
    case Feature::Convert: result = planConvert(request); break;
    case Feature::Audio: result = planAudioEdit(request); break;
    case Feature::Merge: result = planMerge(request); break;
    }
    if (result.plan && request.feature != Feature::Merge && request.feature != Feature::Trim && request.media.durationSeconds) {
        for (auto& step : result.plan->steps) {
            if (step.kind == StepKind::Ffmpeg) step.durationSeconds = *request.media.durationSeconds;
        }
    }
    return result;
}

}  // namespace MediaTool
