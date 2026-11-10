#pragma once

#include "mediatool/bundle.hpp"
#include "mediatool/task_runner.hpp"

#include <QObject>
#include <QString>
#include <QStringList>

namespace MediaTool {

class Engine : public QObject {
    Q_OBJECT
public:
    explicit Engine(Bundle bundle, QObject* parent = nullptr);
    void startup();
    void probeFile(const QString& path);
    void run(JobRequest job);
    void requestStop();
    void requestSkip();
    const EncoderAvailability& encoders() const { return encoders_; }

signals:
    void bundleMissing();
    void encodersReady(const QString& h264, const QString& h265, const QString& vp9, const QString& ffmpegVersion);
    void infoReady(const QString& path, const QString& text, bool ok, const QStringList& audios, const QStringList& subtitles, double durationSeconds);
    void progressChanged(const QString& text, int percent);
    void resultReady(const QString& file, const QString& status, const QString& note, const QString& output);
    void jobFinished();

private:
    static QString implName(EncoderImpl impl);
    Bundle bundle_;
    EncoderAvailability encoders_;
    CancelState cancel_;
    std::optional<MediaInfo> known_;
};

}  // namespace MediaTool
