#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "mediatool/ffmpeg_client.hpp"

#include "mediatool/logic.hpp"
#include "mediatool/messages.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>

#define NOMINMAX
#include <windows.h>

namespace MediaTool {
namespace {

QStringList toQArgs(const std::vector<std::string>& arguments) {
    QStringList list;
    for (const auto& argument : arguments) list << QString::fromUtf8(argument.c_str());
    return list;
}

void hideWindow(QProcess& process) {
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
        args->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
        args->startupInfo->wShowWindow = SW_HIDE;
    });
}

std::optional<long long> readWhole(const QJsonValue& value) {
    if (value.isDouble()) {
        const auto number = static_cast<long long>(value.toDouble());
        return number > 0 ? std::optional<long long>(number) : std::nullopt;
    }
    if (value.isString()) {
        bool ok = false;
        const auto number = value.toString().toLongLong(&ok);
        if (ok && number > 0) return number;
    }
    return std::nullopt;
}

std::optional<double> readDouble(const QJsonValue& value) {
    if (value.isDouble()) return value.toDouble();
    if (value.isString()) {
        bool ok = false;
        const auto number = value.toString().toDouble(&ok);
        if (ok) return number;
    }
    return std::nullopt;
}

class JobHandle {
public:
    explicit JobHandle(qint64 pid) {
        job_ = CreateJobObjectW(nullptr, nullptr);
        if (!job_) return;
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &info, sizeof(info));
        HANDLE process = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
        if (process) {
            AssignProcessToJobObject(job_, process);
            CloseHandle(process);
        }
    }
    ~JobHandle() { close(); }
    void close() {
        if (job_) {
            CloseHandle(job_);
            job_ = nullptr;
        }
    }

private:
    HANDLE job_ = nullptr;
};

}  // namespace

std::optional<MediaInfo> probeMedia(const std::string& ffprobe, const std::string& path, std::string& error) {
    QProcess process;
    hideWindow(process);
    process.start(QString::fromStdString(ffprobe), {"-hide_banner", "-loglevel", "error", "-print_format", "json", "-show_format", "-show_streams", "-show_chapters", "-i", QString::fromUtf8(path.c_str())});
    if (!process.waitForStarted(5000) || !process.waitForFinished(60000) || process.exitCode() != 0) {
        error = kCannotRead;
        return std::nullopt;
    }
    const auto document = QJsonDocument::fromJson(process.readAllStandardOutput());
    if (!document.isObject()) {
        error = kCannotRead;
        return std::nullopt;
    }
    MediaInfo info;
    info.path = path;
    const auto root = document.object();
    info.durationSeconds = readDouble(root.value("format").toObject().value("duration"));
    int audioNumber = 0;
    int subtitleNumber = 0;
    bool foundVideo = false;
    for (const auto& value : root.value("streams").toArray()) {
        const auto stream = value.toObject();
        const auto type = stream.value("codec_type").toString();
        const bool attached = stream.value("disposition").toObject().value("attached_pic").toInt() == 1;
        if (type == "attachment" || type == "data" || (type == "video" && attached)) continue;
        FitStream fit;
        fit.absoluteIndex = stream.value("index").toInt();
        fit.codecType = type.toStdString();
        fit.codecName = stream.value("codec_name").toString().toStdString();
        info.fitStreams.push_back(fit);
        if (type == "video" && !foundVideo) {
            foundVideo = true;
            info.width = stream.value("width").toInt();
            info.height = stream.value("height").toInt();
            info.videoCodec = fit.codecName;
            info.videoBitrateBits = readWhole(stream.value("bit_rate"));
            const auto pixel = stream.value("pix_fmt").toString();
            if (!pixel.isEmpty()) info.pixelFormat = pixel.toStdString();
            const auto rate = stream.value("avg_frame_rate").toString().split('/');
            if (rate.size() == 2) {
                bool okA = false;
                bool okB = false;
                const int num = rate[0].toInt(&okA);
                const int den = rate[1].toInt(&okB);
                if (okA && okB && den > 0) {
                    info.frameNumerator = num;
                    info.frameDenominator = den;
                }
            }
        } else if (type == "audio") {
            AudioTrack track;
            track.number = ++audioNumber;
            track.codec = fit.codecName;
            track.channels = stream.value("channels").toInt();
            if (auto sample = readWhole(stream.value("sample_rate"))) track.sampleRate = static_cast<int>(*sample);
            const auto language = stream.value("tags").toObject().value("language").toString();
            if (!language.isEmpty()) track.languageTag = language.toStdString();
            info.audios.push_back(track);
        } else if (type == "subtitle") {
            SubtitleTrack track;
            track.number = ++subtitleNumber;
            track.codec = fit.codecName;
            track.kind = subtitleKindOf(fit.codecName);
            const auto language = stream.value("tags").toObject().value("language").toString();
            if (!language.isEmpty()) track.languageTag = language.toStdString();
            info.subtitles.push_back(track);
        }
    }
    for (const auto& value : root.value("chapters").toArray()) {
        const auto chapter = value.toObject();
        ChapterMark mark;
        mark.startSeconds = readDouble(chapter.value("start_time")).value_or(0);
        const auto title = chapter.value("tags").toObject().value("title").toString();
        if (!title.isEmpty()) mark.title = title.toStdString();
        info.chapters.push_back(mark);
    }
    if (!foundVideo) {
        error = kCannotRead;
        return std::nullopt;
    }
    return info;
}

int runFfmpeg(const std::string& ffmpeg, const std::vector<std::string>& arguments, const std::function<void(int)>& percent,
              const std::function<bool()>& canceled, double durationSeconds) {
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    hideWindow(process);
    process.start(QString::fromStdString(ffmpeg), toQArgs(arguments));
    if (!process.waitForStarted(8000)) return 1;
    JobHandle job(process.processId());
    QByteArray errors;
    QByteArray pending;
    const QRegularExpression timePattern(R"(out_time=(\d+):(\d+):(\d+(?:\.\d+)?))");
    while (!process.waitForFinished(100)) {
        if (canceled && canceled()) {
            job.close();
            process.kill();
            process.waitForFinished(5000);
            return 2;
        }
        errors.append(process.readAllStandardError());
        if (errors.size() > 65536) errors.resize(65536);
        pending.append(process.readAllStandardOutput());
        while (true) {
            const int newline = pending.indexOf('\n');
            if (newline < 0) break;
            const auto line = QString::fromUtf8(pending.left(newline)).trimmed();
            pending.remove(0, newline + 1);
            const auto match = timePattern.match(line);
            if (match.hasMatch() && durationSeconds > 0 && percent) {
                const double seconds = match.captured(1).toDouble() * 3600.0 + match.captured(2).toDouble() * 60.0 + match.captured(3).toDouble();
                int value = static_cast<int>(seconds / durationSeconds * 100.0);
                if (value < 0) value = 0;
                if (value > 100) value = 100;
                percent(value);
            }
        }
    }
    if (canceled && canceled()) return 2;
    return process.exitCode() == 0 ? 0 : 1;
}

std::string ffmpegVersion(const std::string& ffprobe) {
    QProcess process;
    hideWindow(process);
    process.start(QString::fromStdString(ffprobe), {"-version"});
    if (!process.waitForStarted(5000) || !process.waitForFinished(8000)) return {};
    return process.readAllStandardOutput().split('\n').value(0).trimmed().toStdString();
}

bool probeEncoders(const std::string& ffmpeg, EncoderAvailability& available, const std::function<void(const std::string&)>& status) {
    available = {};
    QProcess list;
    hideWindow(list);
    list.start(QString::fromStdString(ffmpeg), {"-hide_banner", "-loglevel", "error", "-encoders"});
    if (!list.waitForStarted(5000) || !list.waitForFinished(15000)) return false;
    const auto text = QString::fromUtf8(list.readAllStandardOutput());
    auto has = [&](const char* name) { return text.contains(QRegularExpression(QString("\\b%1\\b").arg(name))); };
    auto usable = [&](const char* codec, const QStringList& speed) {
        if (status) status(std::string("正在检测视频编码器…"));
        QProcess process;
        hideWindow(process);
        QStringList args{"-hide_banner", "-loglevel", "error", "-f", "lavfi", "-i", "color=c=black:s=256x256:r=30:d=0.1", "-frames:v", "1", "-an", "-c:v", codec};
        args << speed << "-pix_fmt" << "yuv420p" << "-f" << "null" << "-";
        process.start(QString::fromStdString(ffmpeg), args);
        if (!process.waitForStarted(5000)) return false;
        if (!process.waitForFinished(8000)) {
            process.kill();
            process.waitForFinished(2000);
            return false;
        }
        return process.exitCode() == 0;
    };
    if (has("h264_nvenc") && usable("h264_nvenc", {"-preset", "p5"})) available.h264Nvenc = true;
    else if (has("h264_qsv") && usable("h264_qsv", {"-preset", "medium"})) available.h264Qsv = true;
    else if (has("h264_amf") && usable("h264_amf", {"-quality", "balanced"})) available.h264Amf = true;
    if (has("hevc_nvenc") && usable("hevc_nvenc", {"-preset", "p5"})) available.h265Nvenc = true;
    else if (has("hevc_qsv") && usable("hevc_qsv", {"-preset", "medium"})) available.h265Qsv = true;
    else if (has("hevc_amf") && usable("hevc_amf", {"-quality", "balanced"})) available.h265Amf = true;
    if (has("vp9_qsv") && usable("vp9_qsv", {"-preset", "medium"})) available.vp9Qsv = true;
    if (!has("libx264") || !has("libx265") || !has("libvpx-vp9")) return false;
    return true;
}

}  // namespace MediaTool
