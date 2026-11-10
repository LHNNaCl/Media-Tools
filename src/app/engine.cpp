#include "engine.hpp"

#include "mediatool/ffmpeg_client.hpp"
#include "mediatool/logic.hpp"
#include "mediatool/messages.hpp"

namespace MediaTool {

Engine::Engine(Bundle bundle, QObject* parent) : QObject(parent), bundle_(std::move(bundle)) {}

QString Engine::implName(EncoderImpl impl) {
    switch (impl) {
    case EncoderImpl::Nvenc: return "NVIDIA NVENC";
    case EncoderImpl::Qsv: return "Intel QSV";
    case EncoderImpl::Amf: return "AMD AMF";
    case EncoderImpl::Cpu: return "CPU";
    }
    return "CPU";
}

void Engine::startup() {
    if (!bundle_.valid) {
        emit bundleMissing();
        return;
    }
    const auto version = ffmpegVersion(toUtf8(bundle_.ffprobe));
    const bool ok = probeEncoders(toUtf8(bundle_.ffmpeg), encoders_, [](const std::string&) {});
    if (!ok || version.empty()) {
        emit bundleMissing();
        return;
    }
    emit encodersReady(implName(selectEncoder(VideoEncoder::H264, encoders_)), implName(selectEncoder(VideoEncoder::H265, encoders_)),
                       implName(selectEncoder(VideoEncoder::Vp9, encoders_)), QString::fromStdString(version));
}

void Engine::probeFile(const QString& path) {
    std::string error;
    auto info = probeMedia(toUtf8(bundle_.ffprobe), path.toStdString(), error);
    if (!info) {
        known_.reset();
        emit infoReady(path, QString::fromUtf8(kCannotRead), false, {}, {}, -1);
        return;
    }
    known_ = *info;
    QStringList audios;
    QStringList subtitles;
    for (const auto& track : info->audios) audios << QString("第 %1 路").arg(track.number);
    for (const auto& track : info->subtitles) subtitles << QString("第 %1 路").arg(track.number);
    emit infoReady(path, QString::fromUtf8(formatInfo(*info).c_str()), true, audios, subtitles, info->durationSeconds.value_or(-1));
}

void Engine::run(JobRequest job) {
    cancel_.stop.store(false);
    cancel_.skip.store(false);
    if (known_ && job.files.size() == 1 && job.folderPath.empty() && known_->path == job.files.front()) job.known = known_;
    job.encoders = encoders_;
    job.chooseEncoder = true;
    TaskRunner runner;
    runner.cancel = &cancel_;
    runner.probe = [&](const std::string& path, std::string& error) { return probeMedia(toUtf8(bundle_.ffprobe), path, error); };
    runner.run = [&](const std::vector<std::string>& arguments, double duration, const std::function<void(int)>& percent, const std::function<bool()>& canceled) {
        return runFfmpeg(toUtf8(bundle_.ffmpeg), arguments, percent, canceled, duration);
    };
    runner.onProgress = [&](const ProgressInfo& info) {
        const QString name = QString::fromUtf8(info.name.c_str());
        const QString tail = info.percent < 0 ? QString("进行中") : QString::number(info.percent) + "%";
        QString text;
        if (info.merge) text = QString("正在合并 %1/%2：%3  %4").arg(info.index).arg(info.count).arg(name, tail);
        else if (info.count > 1) text = QString("正在处理 %1/%2：%3  %4").arg(info.index).arg(info.count).arg(name, tail);
        else text = QString("正在处理：%1  %2").arg(name, tail);
        emit progressChanged(text, info.percent);
    };
    const auto results = runner.execute(job);
    for (const auto& result : results) {
        emit resultReady(QString::fromUtf8(result.inputName.c_str()), QString::fromUtf8(statusText(result.status)),
                         QString::fromUtf8(result.message.c_str()), QString::fromUtf8(result.outputPath.c_str()));
    }
    emit jobFinished();
}

void Engine::requestStop() { cancel_.stop.store(true); }
void Engine::requestSkip() { cancel_.skip.store(true); }

}  // namespace MediaTool
