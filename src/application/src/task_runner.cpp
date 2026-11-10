#include "mediatool/task_runner.hpp"

#include "mediatool/logic.hpp"
#include "mediatool/messages.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace MediaTool {
namespace {

bool videoExtension(const std::string& extension) {
    const auto ext = lowerCopy(extension);
    return ext == ".mp4" || ext == ".mkv" || ext == ".avi" || ext == ".mov" || ext == ".webm";
}

void deleteIfPresent(const std::string& path, const std::vector<std::string>& inputs, std::string& note) {
    if (path.empty()) return;
    for (const auto& input : inputs) {
        if (isSamePath(path, input)) return;
    }
    const auto file = utf8Path(path);
    std::error_code error;
    if (!std::filesystem::exists(file, error)) return;
    std::filesystem::remove(file, error);
    if (std::filesystem::exists(file)) note += "残留文件：" + path;
}

void cleanupPlan(const ExecutionPlan& plan, const std::vector<std::string>& inputs, const std::string& tempDir, std::string& note) {
    for (const auto& step : plan.steps) deleteIfPresent(step.outputPath, inputs, note);
    if (!tempDir.empty()) {
        std::error_code error;
        std::filesystem::remove_all(utf8Path(tempDir), error);
    }
}

enum class StepCode { Ok, Fail, Canceled };

void applyEncoder(PlanRequest& request, const JobRequest& job, const std::string& path) {
    if (!job.chooseEncoder) return;
    VideoEncoder family = request.videoEncoder;
    if (request.feature == Feature::Trim) {
        family = extensionOf(path) == ".webm" ? VideoEncoder::Vp9 : VideoEncoder::H264;
    } else if (request.feature == Feature::Merge) {
        family = request.container == ContainerKind::Webm ? VideoEncoder::Vp9 : VideoEncoder::H264;
        if (request.container == ContainerKind::Webm) request.videoEncoder = VideoEncoder::Vp9;
    } else if (request.feature == Feature::Convert && request.convertMode == ConvertMode::Compress) {
        if (request.container == ContainerKind::Webm) {
            family = VideoEncoder::Vp9;
            request.videoEncoder = VideoEncoder::Vp9;
        } else {
            family = request.videoEncoder;
        }
    } else {
        return;
    }
    request.encoderImpl = selectEncoder(family, job.encoders);
}

StepCode runPlan(const ExecutionPlan& plan, const TaskRunner& runner, const std::vector<std::string>&, ProgressInfo& progress, std::string& note) {
    const auto canceled = [&] { return runner.cancel && (runner.cancel->stop.load() || runner.cancel->skip.load()); };
    for (const auto& step : plan.steps) {
        if (canceled()) return StepCode::Canceled;
        if (step.kind == StepKind::WriteText) {
            std::ofstream stream(utf8Path(step.outputPath), std::ios::binary);
            if (!stream) {
                note = kCannotWrite;
                return StepCode::Fail;
            }
            stream << step.textContent.value_or("");
            if (!stream) {
                note = kCannotWrite;
                return StepCode::Fail;
            }
            continue;
        }
        const int code = runner.run(step.arguments, step.durationSeconds, [&](int percent) {
            progress.percent = percent;
            if (runner.onProgress) runner.onProgress(progress);
        }, canceled);
        if (code == 2 || canceled()) return StepCode::Canceled;
        std::error_code error;
        if (code != 0 || !std::filesystem::exists(utf8Path(step.outputPath), error)) return StepCode::Fail;
    }
    return StepCode::Ok;
}

}  // namespace

std::vector<std::string> listVideos(const std::string& directory) {
    std::vector<std::string> files;
    std::error_code error;
    const auto dir = utf8Path(directory);
    if (!std::filesystem::is_directory(dir, error)) return files;
    for (const auto& entry : std::filesystem::directory_iterator(dir, error)) {
        if (error || !entry.is_regular_file()) continue;
        if (videoExtension(toUtf8(entry.path().extension()))) files.push_back(toUtf8(entry.path()));
    }
    return naturalSort(std::move(files));
}

std::vector<ItemResult> TaskRunner::execute(const JobRequest& job) {
    std::vector<std::string> inputs = job.files;
    if (!job.folderPath.empty()) inputs = listVideos(job.folderPath);
    if (!job.folderPath.empty() && inputs.empty()) {
        return {{"", ItemStatus::Failed, kNoVideoInFolder, ""}};
    }
    if (job.options.feature == Feature::Merge && inputs.size() < 2) {
        return {{"", ItemStatus::Failed, kNeedTwo, ""}};
    }

    auto report = [&](int index, int count, const std::string& name, int percent) {
        if (!onProgress) return;
        ProgressInfo info;
        info.index = index;
        info.count = count;
        info.name = name;
        info.percent = percent;
        info.merge = job.options.feature == Feature::Merge;
        onProgress(info);
    };

    if (job.options.feature == Feature::Merge) {
        const bool folder = !job.folderPath.empty();
        const std::string directory = folder ? job.folderPath : parentOf(inputs.front());
        const std::string stem = folder ? stemOf(job.folderPath) : stemOf(inputs.front());
        PlanRequest request = job.options;
        request.feature = Feature::Merge;
        for (const auto& input : inputs) {
            std::string error;
            auto info = probe ? probe(input, error) : std::nullopt;
            if (!info) {
                return {{fileNameOf(input), ItemStatus::Failed, cannotReadNamed(fileNameOf(input)), ""}};
            }
            request.mergeInputs.push_back(*info);
        }
        if (!directoryWritable(directory)) return {{fileNameOf(inputs.front()), ItemStatus::Failed, kCannotWrite, ""}};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto tempDir = toUtf8(std::filesystem::temp_directory_path() / "MediaTool" / std::to_string(stamp));
        std::filesystem::create_directories(utf8Path(tempDir));
        request.tempDir = tempDir;
        applyEncoder(request, job, "");
        request.outputPath = allocateOutput(directory, stem, outputSuffix(Feature::Merge, ConvertMode::Remux, AudioEditMode::Remove), outputExtensionFor(request));
        auto planned = planJob(request);
        if (!planned.plan) {
            std::filesystem::remove_all(utf8Path(tempDir));
            return {{fileNameOf(inputs.front()), ItemStatus::Failed, planned.failureMessage.value_or(kCorrupt), ""}};
        }
        ProgressInfo progress{1, static_cast<int>(inputs.size()), fileNameOf(inputs.front()), -1, true};
        if (onProgress) onProgress(progress);
        std::string note;
        auto code = runPlan(*planned.plan, *this, inputs, progress, note);
        if (code == StepCode::Fail && planned.plan->usesHardware) {
            cleanupPlan(*planned.plan, inputs, tempDir, note);
            std::filesystem::create_directories(utf8Path(tempDir));
            request.encoderImpl = EncoderImpl::Cpu;
            planned = planJob(request);
            if (!planned.plan) {
                std::filesystem::remove_all(utf8Path(tempDir));
                return {{fileNameOf(inputs.front()), ItemStatus::Failed, planned.failureMessage.value_or(kCorrupt), ""}};
            }
            note.clear();
            code = runPlan(*planned.plan, *this, inputs, progress, note);
        }
        if (code != StepCode::Ok) {
            cleanupPlan(*planned.plan, inputs, tempDir, note);
            const auto status = code == StepCode::Canceled ? ItemStatus::Canceled : ItemStatus::Failed;
            const std::string message = code == StepCode::Canceled ? std::string(kCanceled) + note : (note.empty() ? std::string(kCorrupt) : note);
            return {{fileNameOf(inputs.front()), status, message, ""}};
        }
        std::filesystem::remove_all(utf8Path(tempDir));
        return {{fileNameOf(inputs.front()), ItemStatus::Succeeded, planned.plan->successMessage, request.outputPath}};
    }

    std::vector<ItemResult> results;
    const int count = static_cast<int>(inputs.size());
    for (int index = 0; index < count; ++index) {
        const auto& input = inputs[static_cast<size_t>(index)];
        if (cancel && cancel->stop.load()) {
            results.push_back({fileNameOf(input), ItemStatus::NotProcessed, kNotProcessed, ""});
            continue;
        }
        report(index + 1, count, fileNameOf(input), -1);
        std::string error;
        std::optional<MediaInfo> info;
        if (job.known && inputs.size() == 1 && job.known->path == input) info = job.known;
        else if (probe) info = probe(input, error);
        if (!info) {
            results.push_back({fileNameOf(input), ItemStatus::Failed, kCannotRead, ""});
            continue;
        }
        PlanRequest request = job.options;
        request.media = *info;
        applyEncoder(request, job, input);
        if (request.feature == Feature::MuxSubtitle && !job.folderPath.empty()) {
            if (auto sidecar = findSidecar(input, {".srt", ".ass", ".vtt"})) request.externalPath = *sidecar;
            else {
                results.push_back({fileNameOf(input), ItemStatus::Failed, kNoSidecarSub, ""});
                continue;
            }
        }
        if (request.feature == Feature::Audio && request.audioEdit == AudioEditMode::Replace && !job.folderPath.empty()) {
            if (auto sidecar = findSidecar(input, {".m4a", ".mp3", ".wav", ".flac"})) request.externalPath = *sidecar;
            else {
                results.push_back({fileNameOf(input), ItemStatus::Failed, kNoSidecarAudio, ""});
                continue;
            }
        }
        const auto directory = parentOf(input);
        if (!directoryWritable(directory)) {
            results.push_back({fileNameOf(input), ItemStatus::Failed, kCannotWrite, ""});
            continue;
        }
        request.outputPath = allocateOutput(directory, stemOf(input), outputSuffix(request.feature, request.convertMode, request.audioEdit), outputExtensionFor(request));
        auto planned = planJob(request);
        if (!planned.plan) {
            results.push_back({fileNameOf(input), ItemStatus::Failed, planned.failureMessage.value_or(kCorrupt), ""});
            continue;
        }
        ProgressInfo progress{index + 1, count, fileNameOf(input), -1, false};
        if (onProgress) onProgress(progress);
        std::string note;
        auto code = runPlan(*planned.plan, *this, inputs, progress, note);
        if (code == StepCode::Fail && planned.plan->usesHardware) {
            cleanupPlan(*planned.plan, inputs, "", note);
            request.encoderImpl = EncoderImpl::Cpu;
            request.outputPath = allocateOutput(directory, stemOf(input), outputSuffix(request.feature, request.convertMode, request.audioEdit), outputExtensionFor(request));
            planned = planJob(request);
            if (!planned.plan) {
                results.push_back({fileNameOf(input), ItemStatus::Failed, planned.failureMessage.value_or(kCorrupt), ""});
                continue;
            }
            note.clear();
            code = runPlan(*planned.plan, *this, inputs, progress, note);
        }
        if (code == StepCode::Ok) {
            results.push_back({fileNameOf(input), ItemStatus::Succeeded, planned.plan->successMessage, request.outputPath});
            continue;
        }
        cleanupPlan(*planned.plan, inputs, "", note);
        if (code == StepCode::Canceled) {
            results.push_back({fileNameOf(input), ItemStatus::Canceled, std::string(kCanceled) + note, ""});
            if (!cancel || cancel->stop.load() || inputs.size() == 1) {
                for (int rest = index + 1; rest < count; ++rest) {
                    results.push_back({fileNameOf(inputs[static_cast<size_t>(rest)]), ItemStatus::NotProcessed, kNotProcessed, ""});
                }
                break;
            }
            cancel->skip.store(false);
            continue;
        }
        results.push_back({fileNameOf(input), ItemStatus::Failed, note.empty() ? std::string(kCorrupt) : note, ""});
    }
    return results;
}

}  // namespace MediaTool
