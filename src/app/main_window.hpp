#pragma once

#include "engine.hpp"

#include <QMainWindow>

class QButtonGroup;
class QCloseEvent;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QListWidget;
class QMenu;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QSlider;
class QStackedWidget;
class QSystemTrayIcon;
class QTableWidget;
class QThread;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(MediaTool::Bundle bundle, QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void setupTray();
    void showMainWindow();
    void quitFromTray();
    void refresh();
    void setBusy(bool busy);
    void browse();
    void syncInfo();
    void probePath(const QString& path);
    void startJob();
    void cancelJob();
    void showAbout();
    void copyInfo();
    void copyList();
    void addResult(const QString& file, const QString& status, const QString& note, const QString& output);
    void shutdownThread();
    void updateHeader();
    void setStatusChip(const QString& text, bool ready);
    MediaTool::JobRequest buildJob() const;
    QString validate() const;
    int featureIndex() const;
    static void fillTracks(QComboBox* box, const QStringList& items);
    static int trackFrom(const QComboBox* box);

    MediaTool::Bundle bundle_;
    MediaTool::Engine* engine_ = nullptr;
    QThread* thread_ = nullptr;
    QSystemTrayIcon* tray_ = nullptr;
    QMenu* trayMenu_ = nullptr;
    bool ready_ = false;
    bool busy_ = false;
    bool quitting_ = false;
    bool refreshing_ = false;
    double duration_ = -1;

    QFrame* sidebar_ = nullptr;
    QListWidget* featureNav_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* subtitle_ = nullptr;
    QLabel* statusChip_ = nullptr;
    QFrame* inputCard_ = nullptr;
    QFrame* paramCard_ = nullptr;
    QFrame* progressCard_ = nullptr;
    QFrame* resultsCard_ = nullptr;
    QFrame* infoCard_ = nullptr;
    QRadioButton* single_ = nullptr;
    QRadioButton* folder_ = nullptr;
    QStackedWidget* inputStack_ = nullptr;
    QLineEdit* path_ = nullptr;
    QListWidget* files_ = nullptr;
    QPushButton* browse_ = nullptr;
    QLabel* inputIssue_ = nullptr;
    QStackedWidget* params_ = nullptr;
    QLabel* paramIssue_ = nullptr;
    QComboBox* audioTrack_ = nullptr;
    QButtonGroup* audioFormatGroup_ = nullptr;
    QSlider* audioVolume_ = nullptr;
    QLabel* audioVolumeValue_ = nullptr;
    QComboBox* subTrack_ = nullptr;
    QComboBox* subFormat_ = nullptr;
    QLineEdit* subtitlePath_ = nullptr;
    QPushButton* subtitleBrowse_ = nullptr;
    QLabel* muxHint_ = nullptr;
    QRadioButton* trimEndMode_ = nullptr;
    QRadioButton* trimDurationMode_ = nullptr;
    QLineEdit* trimStart_ = nullptr;
    QLineEdit* trimEnd_ = nullptr;
    QLineEdit* trimDuration_ = nullptr;
    QComboBox* mergeContainer_ = nullptr;
    QComboBox* convertMode_ = nullptr;
    QComboBox* convertContainer_ = nullptr;
    QComboBox* resolution_ = nullptr;
    QComboBox* encoder_ = nullptr;
    QLineEdit* bitrate_ = nullptr;
    QComboBox* audioMode_ = nullptr;
    QComboBox* keepTrack_ = nullptr;
    QSlider* editVolume_ = nullptr;
    QLabel* editVolumeValue_ = nullptr;
    QLineEdit* audioPath_ = nullptr;
    QPushButton* audioBrowse_ = nullptr;
    QLabel* audioHint_ = nullptr;
    QLabel* progressText_ = nullptr;
    QLabel* progressPercent_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QPushButton* start_ = nullptr;
    QPushButton* cancel_ = nullptr;
    QTableWidget* results_ = nullptr;
    QPushButton* copyList_ = nullptr;
    QPlainTextEdit* info_ = nullptr;
    QPushButton* copyInfo_ = nullptr;
    QPushButton* about_ = nullptr;
    QString h264Name_ = "CPU";
    QString h265Name_ = "CPU";
    QString vp9Name_ = "CPU";
    QString ffmpegVersion_;
};
