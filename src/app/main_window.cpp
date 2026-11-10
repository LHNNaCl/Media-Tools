#include "main_window.hpp"

#include "mediatool/logic.hpp"
#include "mediatool/messages.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QFileDialog>
#include <QFont>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPalette>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSize>
#include <QSizePolicy>
#include <QSlider>
#include <QStackedWidget>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTableWidget>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QMenu>

#include <filesystem>
#include <iterator>

#ifndef MEDIATOOL_VERSION
#define MEDIATOOL_VERSION "1.0.0"
#endif

namespace {
const char* kVideoFilter = "视频 (*.mp4 *.mkv *.avi *.mov *.webm)";
const char* kSubtitleFilter = "字幕 (*.srt *.ass *.vtt)";
const char* kAudioFilter = "音频 (*.mp3 *.m4a *.wav *.flac)";
const char* kFolderInfo = "选择单个视频后显示媒体信息";

struct FeatureMeta {
    const char* name;
    const char* subtitle;
    char16_t glyph;
};

const FeatureMeta kFeatures[] = {
    {"媒体信息", "查看并导出视频的时长、分辨率、音轨、字幕和章节", u'\uE946'},
    {"提取音频", "从视频中抽出所选音轨，按固定质量保存为 MP3、M4A、WAV 或 FLAC", u'\uE8D6'},
    {"提取字幕", "导出文本字幕，或把图像字幕按原样抽出", u'\uE7B3'},
    {"封装字幕", "把外挂文本字幕封装进 MKV，成为可开关的软字幕", u'\uE898'},
    {"裁剪", "按开始和结束时间，或开始和时长裁出一段视频", u'\uE7A8'},
    {"合并", "按指定顺序把多个视频拼接成一个文件", u'\uE8EE'},
    {"换容器或压缩", "只换容器，或按分辨率、编码器和码率重新压缩", u'\uE72C'},
    {"音轨", "去掉、保留或替换视频里的音轨，并可调节音量", u'\uE9F5'},
};

QFont uiFont(int pointSize = 12, QFont::Weight weight = QFont::Normal) {
    QFont font("Microsoft YaHei UI", pointSize, weight);
    return font;
}

QFont iconFont(int pointSize = 11) {
    QFont font("Segoe MDL2 Assets", pointSize);
    return font;
}

QWidget* formHost(QFormLayout** form) {
    auto* page = new QWidget;
    page->setFont(uiFont());
    *form = new QFormLayout(page);
    (*form)->setContentsMargins(0, 0, 0, 0);
    (*form)->setHorizontalSpacing(12);
    (*form)->setVerticalSpacing(8);
    (*form)->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return page;
}

QFrame* makeCard() {
    auto* card = new QFrame;
    card->setObjectName("card");
    card->setAttribute(Qt::WA_StyledBackground, true);
    return card;
}

QLabel* sectionTitle(const QString& text) {
    auto* label = new QLabel(text);
    label->setObjectName("cardTitle");
    label->setFont(uiFont(12, QFont::DemiBold));
    return label;
}

QWidget* makeSegmentBar(QRadioButton* first, QRadioButton* second) {
    auto* bar = new QFrame;
    bar->setObjectName("segBar");
    bar->setAttribute(Qt::WA_StyledBackground, true);
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);
    first->setObjectName("segButton");
    second->setObjectName("segButton");
    first->setCursor(Qt::PointingHandCursor);
    second->setCursor(Qt::PointingHandCursor);
    first->setFont(uiFont(11));
    second->setFont(uiFont(11));
    layout->addWidget(first);
    layout->addWidget(second);
    return bar;
}

QPushButton* makeFormatButton(const QString& text) {
    auto* button = new QPushButton(text);
    button->setObjectName("formatButton");
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setFont(uiFont(11));
    button->setMinimumHeight(24);
    button->setMinimumWidth(0);
    button->setMaximumHeight(24);
    button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    return button;
}

QWidget* makeVolumeRow(QSlider** slider, QLabel** value) {
    auto* host = new QWidget;
    auto* layout = new QHBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    *slider = new QSlider(Qt::Horizontal);
    (*slider)->setRange(0, 200);
    (*slider)->setValue(100);
    (*slider)->setObjectName("volumeSlider");
    (*slider)->setFixedHeight(16);
    (*slider)->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    *value = new QLabel("100");
    (*value)->setObjectName("volumeValue");
    (*value)->setAlignment(Qt::AlignCenter);
    (*value)->setFixedWidth(34);
    layout->addWidget(*slider, 1);
    layout->addWidget(*value);
    return host;
}

void applyInkTheme() {
    QPalette palette;
    const QColor page("#f1f1f7");
    const QColor ink("#34364a");
    const QColor muted("#7c7e94");
    const QColor fill("#6a6f9a");
    const QColor onFill("#ffffff");
    palette.setColor(QPalette::Window, page);
    palette.setColor(QPalette::WindowText, ink);
    palette.setColor(QPalette::Base, QColor("#ffffff"));
    palette.setColor(QPalette::AlternateBase, QColor("#f5f5fa"));
    palette.setColor(QPalette::Text, ink);
    palette.setColor(QPalette::Button, QColor("#ffffff"));
    palette.setColor(QPalette::ButtonText, ink);
    palette.setColor(QPalette::Highlight, fill);
    palette.setColor(QPalette::HighlightedText, onFill);
    palette.setColor(QPalette::PlaceholderText, muted);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, muted);
    palette.setColor(QPalette::Disabled, QPalette::Text, muted);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, muted);
    qApp->setPalette(palette);
    qApp->setFont(uiFont(11));
    qApp->setStyleSheet(QStringLiteral(R"(
QMainWindow, QDialog, QMessageBox { background: #f1f1f7; color: #34364a; }
QWidget { color: #34364a; }
QLabel#navIcon, QLabel#stateIcon { font-family: "Segoe MDL2 Assets"; }

QFrame#sidebar {
    background: #4b4f6e;
    border: none;
}
QLabel#navLabel { color: #9a9dbb; letter-spacing: 3px; }

QListWidget#featureNav {
    background: transparent;
    border: none;
    outline: none;
    padding: 0 10px;
}
QListWidget#featureNav::item {
    height: 34px;
    margin: 1px 0;
    border-radius: 8px;
    border: 1px solid transparent;
}
QListWidget#featureNav::item:hover { background: rgba(255,255,255,0.06); }
QListWidget#featureNav::item:selected {
    background: #5c6084;
    border: 1px solid #6a6e94;
}
QLabel#navIcon, QLabel#navText { background: transparent; }

QPushButton#aboutSide {
    background: #565a7c;
    color: #d5d7ea;
    border: 1px solid #646896;
    border-radius: 7px;
    min-height: 26px;
    max-height: 26px;
    padding: 0 10px;
    font-size: 11px;
}
QPushButton#aboutSide:hover { background: #5f6388; color: #ffffff; }

QLabel#pageTitle { color: #34364a; font-weight: 700; }
QLabel#pageSubtitle { color: #7c7e94; }
QLabel#statusChip {
    background: #e8e9f3;
    color: #60648c;
    border: 1px solid #d8d9e8;
    border-radius: 999px;
    padding: 2px 10px;
    font-weight: 600;
}
QLabel#statusChip[ready="false"] {
    background: #f3e8e6;
    color: #9a5a52;
    border: 1px solid #e6d2ce;
}

QFrame#card {
    background: #ffffff;
    border: 1px solid #e2e2ed;
    border-radius: 10px;
}
QLabel#cardHint { color: #a0a2b5; font: 10pt "Microsoft YaHei UI"; }

QFrame#segBar {
    background: #e9eaf3;
    border-radius: 10px;
}
QRadioButton#segButton {
    spacing: 0;
    padding: 2px 8px;
    color: #7c7e94;
    border-radius: 7px;
    background: transparent;
    font-size: 11px;
}
QRadioButton#segButton::indicator { width: 0; height: 0; border: none; }
QRadioButton#segButton:checked {
    background: #ffffff;
    color: #60648c;
    font-weight: 600;
}

QLineEdit, QComboBox, QListWidget#fileList {
    background: #f8f8fc;
    color: #34364a;
    border: 1px solid #e1e1ec;
    border-radius: 7px;
    padding: 3px 8px;
    min-height: 16px;
    font: 11pt "Microsoft YaHei UI";
    selection-background-color: #e8e9f3;
    selection-color: #34364a;
}
QLineEdit:focus, QComboBox:focus, QListWidget#fileList:focus { border-color: #6a6f9a; }
QLineEdit:read-only { background: #f8f8fc; }
QComboBox::drop-down { border: none; width: 28px; }
QComboBox QAbstractItemView {
    background: #ffffff;
    color: #34364a;
    selection-background-color: #e8e9f3;
    selection-color: #34364a;
    border: 1px solid #e1e1ec;
    outline: 0;
}

QPushButton {
    background: #ffffff;
    color: #34364a;
    border: 1px solid #e2e2ed;
    border-radius: 7px;
    padding: 2px 10px;
    min-height: 24px;
    min-width: 48px;
    font: 11pt "Microsoft YaHei UI";
}
QPushButton:hover { background: #f0f0f7; }
QPushButton:pressed { background: #e8e8f0; }
QPushButton:disabled { color: #a0a2b5; background: #f5f5fa; border-color: #e2e2ed; }

QPushButton#softButton {
    background: #e8e9f3;
    color: #60648c;
    border: 1px solid #e8e9f3;
    font: 600 11pt "Microsoft YaHei UI";
    min-width: 56px;
    min-height: 26px;
    max-height: 26px;
    padding: 0 12px;
}
QPushButton#softButton:hover { background: #dde0ee; }
QPushButton#softButton:disabled { background: #f0f0f7; color: #a0a2b5; border-color: #f0f0f7; }

QPushButton#primaryButton {
    background: #6a6f9a;
    color: #ffffff;
    border: 1px solid #6a6f9a;
    min-width: 56px;
    min-height: 24px;
    max-height: 24px;
    padding: 0 14px;
    font: 600 11pt "Microsoft YaHei UI";
}
QPushButton#primaryButton:hover { background: #5c6088; border-color: #5c6088; }
QPushButton#primaryButton:pressed { background: #52567a; border-color: #52567a; }
QPushButton#primaryButton:disabled {
    background: #d5d7e6;
    color: #9aa0b8;
    border-color: #d5d7e6;
}

QPushButton#copyButton {
    min-height: 22px;
    max-height: 22px;
    min-width: 0;
    padding: 0 8px;
    font-size: 11px;
}

QPushButton#formatButton {
    background: transparent;
    color: #7c7e94;
    border: none;
    border-radius: 6px;
    min-width: 0px;
    max-width: 16777215px;
    min-height: 22px;
    max-height: 24px;
    padding: 2px 2px;
}
QPushButton#formatButton:checked {
    background: #ffffff;
    color: #60648c;
    font-weight: 600;
}
QFrame#formatBar {
    background: #e9eaf3;
    border-radius: 8px;
}

QSlider#volumeSlider {
    min-height: 16px;
    max-height: 16px;
}
QSlider#volumeSlider::groove:horizontal {
    height: 5px;
    background: #e0e1ec;
    border-radius: 3px;
}
QSlider#volumeSlider::sub-page:horizontal {
    background: #6a6f9a;
    border-radius: 3px;
}
QSlider#volumeSlider::handle:horizontal {
    width: 8px;
    height: 12px;
    margin: -3.5px 0;
    background: #6a6f9a;
    border: none;
    border-radius: 2px;
}
QLabel#volumeValue {
    background: #e8e9f3;
    color: #60648c;
    border-radius: 5px;
    font-weight: 700;
    padding: 1px 0;
    font-size: 11px;
}

QLabel#issueLabel { color: #9a5a52; font: 10pt "Microsoft YaHei UI"; }
QLabel#hintLabel { color: #7c7e94; font: 10pt "Microsoft YaHei UI"; }
QLabel#fieldLabel { color: #7c7e94; font: 10pt "Microsoft YaHei UI"; }
QLabel#cardTitle { color: #34364a; font: 600 12pt "Microsoft YaHei UI"; }
QLabel#progressText { color: #34364a; font: 600 11pt "Microsoft YaHei UI"; }
QLabel#progressPercent { color: #60648c; font: 600 11pt "Microsoft YaHei UI"; }
QLabel#stateIcon {
    background: #e8e9f3;
    color: #60648c;
    border-radius: 12px;
}

QProgressBar {
    background: #e0e1ec;
    border: none;
    border-radius: 3px;
    min-height: 6px;
    max-height: 6px;
}
QProgressBar::chunk {
    background: #6a6f9a;
    border-radius: 4px;
}

QPlainTextEdit {
    background: transparent;
    color: #34364a;
    border: none;
    selection-background-color: #e8e9f3;
    selection-color: #34364a;
}
QPlainTextEdit[placeholder="true"] { color: #7c7e94; }

QTableWidget {
    background: transparent;
    border: none;
    gridline-color: transparent;
    outline: none;
}
QTableWidget::item { padding: 5px; color: #34364a; }
QTableWidget::item:selected { background: #e8e9f3; color: #34364a; }
QHeaderView::section {
    background: transparent;
    color: #a0a2b5;
    border: none;
    border-bottom: 1px solid #e2e2ed;
    padding: 4px 6px;
    font-weight: 600;
    font-size: 10px;
}

QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #d0d1de; border-radius: 4px; min-height: 24px; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: #d0d1de; border-radius: 4px; min-width: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QToolTip { background: #ffffff; color: #34364a; border: 1px solid #e2e2ed; }
)"));
}

void fitToCurrentPage(QStackedWidget* stack) {
    for (int index = 0; index < stack->count(); ++index) {
        auto* page = stack->widget(index);
        auto policy = page->sizePolicy();
        policy.setVerticalPolicy(index == stack->currentIndex() ? QSizePolicy::Preferred : QSizePolicy::Ignored);
        page->setSizePolicy(policy);
    }
    stack->updateGeometry();
}

void setInfoText(QPlainTextEdit* edit, const QString& text, bool placeholder) {
    edit->setPlainText(text);
    edit->setProperty("placeholder", placeholder);
    edit->style()->unpolish(edit);
    edit->style()->polish(edit);
}
}  // namespace

MainWindow::MainWindow(MediaTool::Bundle bundle, QWidget* parent) : QMainWindow(parent), bundle_(std::move(bundle)) {
    applyInkTheme();
    setWindowTitle("媒体小工具");
    resize(1180, 720);
    setMinimumSize(920, 600);
    buildUi();
    setupTray();
    refresh();
    setStatusChip("正在检测视频编码器…", false);
    progressText_->setText("正在检测视频编码器…");
    progressPercent_->setText("");
    progress_->setRange(0, 0);
    start_->setEnabled(false);
    cancel_->setEnabled(false);

    thread_ = new QThread(this);
    engine_ = new MediaTool::Engine(bundle_);
    engine_->moveToThread(thread_);
    connect(thread_, &QThread::started, engine_, &MediaTool::Engine::startup);
    connect(thread_, &QThread::finished, engine_, &QObject::deleteLater);
    connect(engine_, &MediaTool::Engine::bundleMissing, this, [this] {
        ready_ = false;
        progress_->setRange(0, 100);
        progress_->setValue(0);
        progressText_->setText(QString::fromUtf8(MediaTool::kNoFfmpeg));
        progressPercent_->setText("");
        setStatusChip(QString::fromUtf8(MediaTool::kNoFfmpeg), false);
        refresh();
    });
    connect(engine_, &MediaTool::Engine::encodersReady, this, [this](const QString& h264, const QString& h265, const QString& vp9, const QString& version) {
        h264Name_ = h264;
        h265Name_ = h265;
        vp9Name_ = vp9;
        ffmpegVersion_ = version;
        ready_ = true;
        if (!busy_) {
            progress_->setRange(0, 100);
            progress_->setValue(0);
            progressText_->setText("就绪");
            progressPercent_->setText("");
        }
        setStatusChip(QString("编码器就绪 · %1").arg(h264Name_), true);
        refresh();
    });
    connect(engine_, &MediaTool::Engine::infoReady, this,
            [this](const QString& path, const QString& text, bool ok, const QStringList& audios, const QStringList& subtitles, double durationSeconds) {
                const QString selected = files_->currentItem() ? files_->currentItem()->text() : QString();
                if (folder_->isChecked() || (path != path_->text() && path != selected)) return;
                duration_ = ok ? durationSeconds : -1;
                setInfoText(info_, text, false);
                if (!folder_->isChecked() && featureIndex() != static_cast<int>(MediaTool::Feature::Merge)) {
                    fillTracks(audioTrack_, audios);
                    fillTracks(keepTrack_, audios);
                    fillTracks(subTrack_, subtitles);
                }
                if (!busy_) {
                    progress_->setRange(0, 100);
                    progress_->setValue(0);
                    progressText_->setText("就绪");
                    progressPercent_->setText("");
                }
                refresh();
            });
    connect(engine_, &MediaTool::Engine::progressChanged, this, [this](const QString& text, int percent) {
        progressText_->setText(text);
        if (percent < 0) {
            progress_->setRange(0, 0);
            progressPercent_->setText("进行中");
        } else {
            progress_->setRange(0, 100);
            progress_->setValue(percent);
            progressPercent_->setText(QString("%1%").arg(percent));
        }
    });
    connect(engine_, &MediaTool::Engine::resultReady, this, &MainWindow::addResult);
    connect(engine_, &MediaTool::Engine::jobFinished, this, [this] {
        setBusy(false);
        progress_->setRange(0, 100);
        progress_->setValue(100);
        progressText_->setText("已完成");
        progressPercent_->setText("100%");
        refresh();
        if (quitting_) close();
    });
    thread_->start();
}

MainWindow::~MainWindow() { shutdownThread(); }

void MainWindow::buildUi() {
    auto* central = new QWidget;
    central->setFont(uiFont());
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    sidebar_ = new QFrame;
    sidebar_->setObjectName("sidebar");
    sidebar_->setFixedWidth(176);
    sidebar_->setAttribute(Qt::WA_StyledBackground, true);
    auto* sideLayout = new QVBoxLayout(sidebar_);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(0);

    auto* navLabel = new QLabel("功能");
    navLabel->setObjectName("navLabel");
    navLabel->setFont(uiFont(10));
    navLabel->setContentsMargins(16, 12, 16, 6);
    sideLayout->addWidget(navLabel);

    featureNav_ = new QListWidget;
    featureNav_->setObjectName("featureNav");
    featureNav_->setFont(uiFont(12));
    featureNav_->setFocusPolicy(Qt::NoFocus);
    featureNav_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    featureNav_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    featureNav_->setSpacing(2);
    for (const auto& feature : kFeatures) {
        auto* item = new QListWidgetItem;
        item->setSizeHint(QSize(156, 34));
        featureNav_->addItem(item);
        auto* row = new QWidget;
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(10, 0, 10, 0);
        rowLayout->setSpacing(8);
        auto* icon = new QLabel(QString(QChar(feature.glyph)));
        icon->setObjectName("navIcon");
        icon->setFont(iconFont(11));
        icon->setFixedWidth(16);
        icon->setAlignment(Qt::AlignCenter);
        auto* text = new QLabel(QString::fromUtf8(feature.name));
        text->setObjectName("navText");
        text->setFont(uiFont(12));
        rowLayout->addWidget(icon);
        rowLayout->addWidget(text, 1);
        featureNav_->setItemWidget(item, row);
    }
    featureNav_->setCurrentRow(1);
    sideLayout->addWidget(featureNav_, 1);

    auto* sideFooter = new QWidget;
    auto* sideFooterLayout = new QVBoxLayout(sideFooter);
    sideFooterLayout->setContentsMargins(10, 6, 10, 12);
    about_ = new QPushButton("关于");
    about_->setObjectName("aboutSide");
    about_->setCursor(Qt::PointingHandCursor);
    about_->setFont(uiFont(11));
    sideFooterLayout->addWidget(about_);
    sideLayout->addWidget(sideFooter);
    root->addWidget(sidebar_);

    auto* mainPane = new QWidget;
    auto* mainLayout = new QVBoxLayout(mainPane);
    mainLayout->setContentsMargins(16, 10, 16, 12);
    mainLayout->setSpacing(0);

    auto* header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 8);
    auto* titleBlock = new QVBoxLayout;
    titleBlock->setSpacing(2);
    title_ = new QLabel;
    title_->setObjectName("pageTitle");
    title_->setFont(uiFont(16, QFont::Bold));
    subtitle_ = new QLabel;
    subtitle_->setObjectName("pageSubtitle");
    subtitle_->setFont(uiFont(10));
    subtitle_->setWordWrap(true);
    subtitle_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    titleBlock->addWidget(title_);
    titleBlock->addWidget(subtitle_);
    header->addLayout(titleBlock, 1);
    statusChip_ = new QLabel;
    statusChip_->setObjectName("statusChip");
    statusChip_->setFont(uiFont(10, QFont::DemiBold));
    statusChip_->setAttribute(Qt::WA_StyledBackground, true);
    statusChip_->setMaximumWidth(200);
    statusChip_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    header->addWidget(statusChip_, 0, Qt::AlignTop);
    mainLayout->addLayout(header);

    auto* columns = new QHBoxLayout;
    columns->setSpacing(10);
    auto* left = new QVBoxLayout;
    left->setSpacing(8);

    inputCard_ = makeCard();
    auto* inputOuter = new QVBoxLayout(inputCard_);
    inputOuter->setContentsMargins(10, 8, 10, 8);
    inputOuter->setSpacing(6);
    auto* inputHead = new QHBoxLayout;
    inputHead->addWidget(sectionTitle("输入"));
    inputHead->addStretch();
    single_ = new QRadioButton("单个视频");
    folder_ = new QRadioButton("文件夹");
    single_->setChecked(true);
    inputHead->addWidget(makeSegmentBar(single_, folder_));
    inputOuter->addLayout(inputHead);

    path_ = new QLineEdit;
    path_->setReadOnly(true);
    path_->setFont(uiFont(11));
    path_->setPlaceholderText("选择视频文件或文件夹");
    files_ = new QListWidget;
    files_->setObjectName("fileList");
    files_->setFixedHeight(140);
    auto* listButtons = new QVBoxLayout;
    listButtons->setSpacing(8);
    auto* up = new QPushButton("上移");
    auto* down = new QPushButton("下移");
    auto* remove = new QPushButton("移除");
    listButtons->addWidget(up);
    listButtons->addWidget(down);
    listButtons->addWidget(remove);
    listButtons->addStretch();
    auto* listPage = new QWidget;
    auto* listLayout = new QHBoxLayout(listPage);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(8);
    listLayout->addWidget(files_, 1);
    listLayout->addLayout(listButtons);
    inputStack_ = new QStackedWidget;
    inputStack_->addWidget(path_);
    inputStack_->addWidget(listPage);
    browse_ = new QPushButton("浏览");
    browse_->setObjectName("softButton");
    browse_->setFont(uiFont(11));
    browse_->setMinimumWidth(56);
    auto* inputRow = new QHBoxLayout;
    inputRow->setSpacing(10);
    inputRow->addWidget(inputStack_, 1);
    inputRow->addWidget(browse_);
    inputOuter->addLayout(inputRow);
    inputIssue_ = new QLabel;
    inputIssue_->setObjectName("issueLabel");
    inputIssue_->setFont(uiFont(10));
    inputIssue_->setWordWrap(true);
    inputOuter->addWidget(inputIssue_);
    left->addWidget(inputCard_);

    paramCard_ = makeCard();
    auto* paramOuter = new QVBoxLayout(paramCard_);
    paramOuter->setContentsMargins(10, 8, 10, 8);
    paramOuter->setSpacing(6);
    auto* paramHead = new QHBoxLayout;
    paramHead->addWidget(sectionTitle("参数"));
    paramHead->addStretch();
    auto* paramHint = new QLabel("输出写在源文件旁边，不修改源文件");
    paramHint->setObjectName("cardHint");
    paramHint->setFont(uiFont(10));
    paramHint->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    paramHint->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    paramHint->setToolTip("输出写在源文件旁边，不修改源文件");
    paramHead->addWidget(paramHint, 1);
    paramOuter->addLayout(paramHead);

    params_ = new QStackedWidget;
    params_->setFont(uiFont());
    auto addPage = [this](QWidget* page) { params_->addWidget(page); };
    addPage(new QWidget);

    QFormLayout* form = nullptr;
    auto* audioPage = new QWidget;
    audioPage->setFont(uiFont());
    auto* audioLayout = new QVBoxLayout(audioPage);
    audioLayout->setContentsMargins(0, 0, 0, 0);
    audioLayout->setSpacing(6);
    audioTrack_ = new QComboBox;
    fillTracks(audioTrack_, {});
    auto* trackRow = new QHBoxLayout;
    trackRow->setSpacing(8);
    auto* trackLabel = new QLabel("音轨");
    trackLabel->setObjectName("fieldLabel");
    trackLabel->setFont(uiFont(10, QFont::DemiBold));
    trackLabel->setMinimumWidth(36);
    trackRow->addWidget(trackLabel);
    audioTrack_->setFont(uiFont(11));
    trackRow->addWidget(audioTrack_, 1);
    audioLayout->addLayout(trackRow);

    auto* formatRow = new QHBoxLayout;
    formatRow->setSpacing(8);
    auto* formatLabel = new QLabel("格式");
    formatLabel->setObjectName("fieldLabel");
    formatLabel->setFont(uiFont(10, QFont::DemiBold));
    formatLabel->setMinimumWidth(36);
    auto* formatBar = new QFrame;
    formatBar->setObjectName("formatBar");
    formatBar->setAttribute(Qt::WA_StyledBackground, true);
    formatBar->setMinimumWidth(0);
    formatBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* formatLayout = new QHBoxLayout(formatBar);
    formatLayout->setContentsMargins(2, 2, 2, 2);
    formatLayout->setSpacing(1);
    audioFormatGroup_ = new QButtonGroup(this);
    const QStringList formats = {"MP3", "M4A", "WAV", "FLAC"};
    for (int index = 0; index < formats.size(); ++index) {
        auto* button = makeFormatButton(formats[index]);
        if (index == 0) button->setChecked(true);
        audioFormatGroup_->addButton(button, index);
        formatLayout->addWidget(button, 1);
    }
    formatRow->addWidget(formatLabel);
    formatRow->addWidget(formatBar, 1);
    audioLayout->addLayout(formatRow);

    auto* volumeHost = makeVolumeRow(&audioVolume_, &audioVolumeValue_);
    auto* volumeRow = new QHBoxLayout;
    volumeRow->setSpacing(8);
    auto* volumeLabel = new QLabel("音量");
    volumeLabel->setObjectName("fieldLabel");
    volumeLabel->setFont(uiFont(10, QFont::DemiBold));
    volumeLabel->setMinimumWidth(36);
    volumeRow->addWidget(volumeLabel);
    volumeRow->addWidget(volumeHost, 1);
    audioLayout->addLayout(volumeRow);
    addPage(audioPage);

    auto* subPage = formHost(&form);
    subTrack_ = new QComboBox;
    subFormat_ = new QComboBox;
    subFormat_->addItems({"SRT", "ASS", "VTT", "原样"});
    fillTracks(subTrack_, {});
    form->addRow("字幕", subTrack_);
    form->addRow("格式", subFormat_);
    addPage(subPage);

    auto* muxPage = formHost(&form);
    subtitlePath_ = new QLineEdit;
    subtitlePath_->setReadOnly(true);
    subtitleBrowse_ = new QPushButton("浏览");
    subtitleBrowse_->setObjectName("softButton");
    subtitleBrowse_->setMinimumWidth(56);
    auto* subtitleRow = new QHBoxLayout;
    subtitleRow->setSpacing(8);
    subtitleRow->addWidget(subtitlePath_, 1);
    subtitleRow->addWidget(subtitleBrowse_);
    auto* subtitleHost = new QWidget;
    subtitleHost->setLayout(subtitleRow);
    muxHint_ = new QLabel("将查找同名的 srt、ass 或 vtt");
    muxHint_->setObjectName("hintLabel");
    muxHint_->setWordWrap(true);
    form->addRow("外部字幕", subtitleHost);
    form->addRow("", muxHint_);
    addPage(muxPage);

    auto* trimPage = formHost(&form);
    trimEndMode_ = new QRadioButton("开始和结束");
    trimDurationMode_ = new QRadioButton("开始和时长");
    trimEndMode_->setChecked(true);
    auto* trimModeHost = makeSegmentBar(trimEndMode_, trimDurationMode_);
    trimStart_ = new QLineEdit;
    trimEnd_ = new QLineEdit;
    trimDuration_ = new QLineEdit;
    form->addRow("", trimModeHost);
    form->addRow("开始", trimStart_);
    form->addRow("结束", trimEnd_);
    form->addRow("时长", trimDuration_);
    addPage(trimPage);

    auto* mergePage = formHost(&form);
    mergeContainer_ = new QComboBox;
    mergeContainer_->addItems({"MP4", "MKV", "WebM"});
    form->addRow("容器", mergeContainer_);
    addPage(mergePage);

    auto* convertPage = formHost(&form);
    convertMode_ = new QComboBox;
    convertMode_->addItems({"只换容器", "压缩"});
    convertContainer_ = new QComboBox;
    convertContainer_->addItems({"MP4", "MKV", "WebM"});
    resolution_ = new QComboBox;
    resolution_->addItems({"原分辨率", "1080p", "720p"});
    encoder_ = new QComboBox;
    encoder_->addItems({"H.264", "H.265", "VP9"});
    bitrate_ = new QLineEdit;
    bitrate_->setPlaceholderText("留空则使用默认质量");
    form->addRow("操作", convertMode_);
    form->addRow("容器", convertContainer_);
    form->addRow("分辨率", resolution_);
    form->addRow("编码器", encoder_);
    form->addRow("码率", bitrate_);
    addPage(convertPage);

    auto* editPage = formHost(&form);
    audioMode_ = new QComboBox;
    audioMode_->addItems({"去掉音轨", "保留音轨", "替换音轨"});
    keepTrack_ = new QComboBox;
    fillTracks(keepTrack_, {});
    auto* editVolumeHost = makeVolumeRow(&editVolume_, &editVolumeValue_);
    audioPath_ = new QLineEdit;
    audioPath_->setReadOnly(true);
    audioBrowse_ = new QPushButton("浏览");
    audioBrowse_->setObjectName("softButton");
    audioBrowse_->setMinimumWidth(56);
    auto* audioRow = new QHBoxLayout;
    audioRow->setSpacing(8);
    audioRow->addWidget(audioPath_, 1);
    audioRow->addWidget(audioBrowse_);
    auto* audioHost = new QWidget;
    audioHost->setLayout(audioRow);
    audioHint_ = new QLabel("将查找同名的 m4a、mp3、wav 或 flac");
    audioHint_->setObjectName("hintLabel");
    audioHint_->setWordWrap(true);
    form->addRow("操作", audioMode_);
    form->addRow("音轨", keepTrack_);
    form->addRow("音量", editVolumeHost);
    form->addRow("外部音频", audioHost);
    form->addRow("", audioHint_);
    addPage(editPage);

    paramIssue_ = new QLabel;
    paramIssue_->setObjectName("issueLabel");
    paramIssue_->setFont(uiFont(10));
    paramIssue_->setWordWrap(true);
    paramOuter->addWidget(params_);
    paramOuter->addWidget(paramIssue_);
    left->addWidget(paramCard_);

    progressCard_ = makeCard();
    auto* progressOuter = new QHBoxLayout(progressCard_);
    progressOuter->setContentsMargins(10, 8, 10, 8);
    progressOuter->setSpacing(8);
    auto* stateIcon = new QLabel(QString(QChar(0xE73E)));
    stateIcon->setObjectName("stateIcon");
    stateIcon->setFont(iconFont(10));
    stateIcon->setFixedSize(24, 24);
    stateIcon->setAlignment(Qt::AlignCenter);
    stateIcon->setAttribute(Qt::WA_StyledBackground, true);
    progressOuter->addWidget(stateIcon);

    auto* progressMain = new QVBoxLayout;
    progressMain->setSpacing(4);
    auto* progressTop = new QHBoxLayout;
    progressText_ = new QLabel("就绪");
    progressText_->setObjectName("progressText");
    progressText_->setFont(uiFont(11, QFont::DemiBold));
    progressPercent_ = new QLabel;
    progressPercent_->setObjectName("progressPercent");
    progressPercent_->setFont(uiFont(11, QFont::Bold));
    progressTop->addWidget(progressText_);
    progressTop->addStretch();
    progressTop->addWidget(progressPercent_);
    progress_ = new QProgressBar;
    progress_->setRange(0, 100);
    progress_->setValue(0);
    progress_->setTextVisible(false);
    progressMain->addLayout(progressTop);
    progressMain->addWidget(progress_);
    progressOuter->addLayout(progressMain, 1);

    start_ = new QPushButton("开始");
    start_->setObjectName("primaryButton");
    start_->setCursor(Qt::PointingHandCursor);
    start_->setFont(uiFont(11, QFont::DemiBold));
    cancel_ = new QPushButton("取消");
    cancel_->setFont(uiFont(11));
    progressOuter->addWidget(start_);
    progressOuter->addWidget(cancel_);
    left->addWidget(progressCard_);

    resultsCard_ = makeCard();
    auto* resultsOuter = new QVBoxLayout(resultsCard_);
    resultsOuter->setContentsMargins(10, 8, 10, 8);
    resultsOuter->setSpacing(4);
    auto* resultsHead = new QHBoxLayout;
    resultsHead->addWidget(sectionTitle("结果清单"));
    resultsHead->addStretch();
    copyList_ = new QPushButton("复制清单");
    copyList_->setObjectName("copyButton");
    copyList_->setCursor(Qt::PointingHandCursor);
    copyList_->setFont(uiFont(10));
    resultsHead->addWidget(copyList_);
    resultsOuter->addLayout(resultsHead);
    results_ = new QTableWidget(0, 4);
    results_->setFont(uiFont(11));
    results_->setHorizontalHeaderLabels({"文件", "结果", "说明", "输出"});
    results_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    results_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    results_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    results_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    results_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    results_->verticalHeader()->setVisible(false);
    results_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    results_->setSelectionBehavior(QAbstractItemView::SelectRows);
    results_->setSelectionMode(QAbstractItemView::SingleSelection);
    results_->setShowGrid(false);
    results_->setFrameShape(QFrame::NoFrame);
    results_->setMinimumHeight(90);
    resultsOuter->addWidget(results_, 1);
    left->addWidget(resultsCard_, 1);

    infoCard_ = makeCard();
    infoCard_->setFixedWidth(240);
    infoCard_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    auto* infoOuter = new QVBoxLayout(infoCard_);
    infoOuter->setContentsMargins(10, 8, 10, 8);
    infoOuter->setSpacing(6);
    auto* infoHead = new QHBoxLayout;
    infoHead->addWidget(sectionTitle("媒体信息"));
    infoHead->addStretch();
    copyInfo_ = new QPushButton("复制信息");
    copyInfo_->setObjectName("copyButton");
    copyInfo_->setCursor(Qt::PointingHandCursor);
    copyInfo_->setFont(uiFont(10));
    infoHead->addWidget(copyInfo_);
    infoOuter->addLayout(infoHead);
    info_ = new QPlainTextEdit;
    info_->setReadOnly(true);
    info_->setFont(uiFont(11));
    infoOuter->addWidget(info_, 1);
    setInfoText(info_, QString::fromUtf8(kFolderInfo), true);

    columns->addLayout(left, 1);
    columns->addWidget(infoCard_);
    mainLayout->addLayout(columns, 1);
    root->addWidget(mainPane, 1);
    setCentralWidget(central);
    updateHeader();

    connect(about_, &QPushButton::clicked, this, &MainWindow::showAbout);
    connect(browse_, &QPushButton::clicked, this, &MainWindow::browse);
    connect(start_, &QPushButton::clicked, this, &MainWindow::startJob);
    connect(cancel_, &QPushButton::clicked, this, &MainWindow::cancelJob);
    connect(copyInfo_, &QPushButton::clicked, this, &MainWindow::copyInfo);
    connect(copyList_, &QPushButton::clicked, this, &MainWindow::copyList);
    connect(featureNav_, &QListWidget::currentRowChanged, this, [this] {
        updateHeader();
        refresh();
        syncInfo();
    });
    connect(single_, &QRadioButton::toggled, this, [this](bool checked) {
        if (!checked) return;
        refresh();
        syncInfo();
    });
    connect(folder_, &QRadioButton::toggled, this, [this](bool checked) {
        if (!checked) return;
        fillTracks(audioTrack_, {});
        fillTracks(subTrack_, {});
        fillTracks(keepTrack_, {});
        refresh();
        syncInfo();
    });
    connect(files_, &QListWidget::currentRowChanged, this, [this] { syncInfo(); });
    connect(up, &QPushButton::clicked, this, [this] {
        const int row = files_->currentRow();
        if (row <= 0) return;
        files_->insertItem(row - 1, files_->takeItem(row));
        files_->setCurrentRow(row - 1);
    });
    connect(down, &QPushButton::clicked, this, [this] {
        const int row = files_->currentRow();
        if (row < 0 || row >= files_->count() - 1) return;
        files_->insertItem(row + 1, files_->takeItem(row));
        files_->setCurrentRow(row + 1);
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        delete files_->takeItem(files_->currentRow());
        refresh();
        syncInfo();
    });
    const auto changed = [this] { refresh(); };
    connect(audioTrack_, &QComboBox::currentIndexChanged, this, changed);
    connect(audioFormatGroup_, &QButtonGroup::idClicked, this, [this](int) { refresh(); });
    connect(audioVolume_, &QSlider::valueChanged, this, [this](int value) {
        audioVolumeValue_->setText(QString::number(value));
        refresh();
    });
    connect(subTrack_, &QComboBox::currentIndexChanged, this, changed);
    connect(subFormat_, &QComboBox::currentIndexChanged, this, changed);
    connect(subtitlePath_, &QLineEdit::textChanged, this, changed);
    connect(trimEndMode_, &QRadioButton::toggled, this, changed);
    connect(trimStart_, &QLineEdit::textChanged, this, changed);
    connect(trimEnd_, &QLineEdit::textChanged, this, changed);
    connect(trimDuration_, &QLineEdit::textChanged, this, changed);
    connect(mergeContainer_, &QComboBox::currentIndexChanged, this, changed);
    connect(convertMode_, &QComboBox::currentIndexChanged, this, changed);
    connect(convertContainer_, &QComboBox::currentIndexChanged, this, changed);
    connect(resolution_, &QComboBox::currentIndexChanged, this, changed);
    connect(encoder_, &QComboBox::currentIndexChanged, this, changed);
    connect(bitrate_, &QLineEdit::textChanged, this, changed);
    connect(audioMode_, &QComboBox::currentIndexChanged, this, changed);
    connect(keepTrack_, &QComboBox::currentIndexChanged, this, changed);
    connect(editVolume_, &QSlider::valueChanged, this, [this](int value) {
        editVolumeValue_->setText(QString::number(value));
        refresh();
    });
    connect(audioPath_, &QLineEdit::textChanged, this, changed);
    connect(subtitleBrowse_, &QPushButton::clicked, this, [this] {
        const auto file = QFileDialog::getOpenFileName(this, "选择字幕", {}, kSubtitleFilter);
        if (!file.isEmpty()) subtitlePath_->setText(file);
    });
    connect(audioBrowse_, &QPushButton::clicked, this, [this] {
        const auto file = QFileDialog::getOpenFileName(this, "选择音频", {}, kAudioFilter);
        if (!file.isEmpty()) audioPath_->setText(file);
    });
}

void MainWindow::updateHeader() {
    const int index = featureIndex();
    if (index < 0 || index >= static_cast<int>(std::size(kFeatures))) return;
    title_->setText(QString::fromUtf8(kFeatures[index].name));
    subtitle_->setText(QString::fromUtf8(kFeatures[index].subtitle));
    for (int row = 0; row < featureNav_->count(); ++row) {
        auto* rowWidget = featureNav_->itemWidget(featureNav_->item(row));
        if (!rowWidget) continue;
        const QString color = row == index ? "#ffffff" : "#c3c6dd";
        const QString weight = row == index ? "600" : "400";
        for (auto* label : rowWidget->findChildren<QLabel*>()) {
            label->setStyleSheet(QString("color:%1; font-weight:%2; background:transparent;").arg(color, weight));
        }
    }
}

void MainWindow::setStatusChip(const QString& text, bool ready) {
    const QFontMetrics metrics(statusChip_->font());
    statusChip_->setToolTip(text);
    statusChip_->setText(metrics.elidedText(text, Qt::ElideRight, statusChip_->maximumWidth() - 20));
    statusChip_->setProperty("ready", ready);
    statusChip_->style()->unpolish(statusChip_);
    statusChip_->style()->polish(statusChip_);
}

int MainWindow::featureIndex() const {
    return featureNav_ ? featureNav_->currentRow() : 0;
}

void MainWindow::fillTracks(QComboBox* box, const QStringList& items) {
    const QString previous = box->currentText();
    box->blockSignals(true);
    box->clear();
    if (items.isEmpty()) {
        for (int number = 1; number <= 8; ++number) box->addItem(QString("第 %1 路").arg(number));
    } else {
        box->addItems(items);
    }
    const int keep = box->findText(previous);
    if (keep >= 0) box->setCurrentIndex(keep);
    box->blockSignals(false);
}

int MainWindow::trackFrom(const QComboBox* box) {
    const auto text = box->currentText();
    int value = 0;
    for (const auto ch : text) {
        if (ch.isDigit()) value = value * 10 + ch.digitValue();
    }
    return value > 0 ? value : 1;
}

QString MainWindow::validate() const {
    using namespace MediaTool;
    const auto feature = static_cast<Feature>(featureIndex());
    const bool folder = folder_->isChecked();
    if (feature == Feature::Merge && !folder) {
        if (files_->count() == 0) return QString::fromUtf8(kPickVideo);
        if (files_->count() < 2) return QString::fromUtf8(kNeedTwo);
    } else if (path_->text().trimmed().isEmpty()) {
        return QString::fromUtf8(kPickVideo);
    }
    if (feature == Feature::ExtractAudio && (audioVolume_->value() < 0 || audioVolume_->value() > 200)) {
        return QString::fromUtf8(kVolumeInvalid);
    }
    if (feature == Feature::Audio && audioMode_->currentIndex() != 0 && (editVolume_->value() < 0 || editVolume_->value() > 200)) {
        return QString::fromUtf8(kVolumeInvalid);
    }
    if (feature == Feature::Trim) {
        std::string error;
        const auto mode = trimEndMode_->isChecked() ? TrimMode::StartEnd : TrimMode::StartDuration;
        const auto other = mode == TrimMode::StartEnd ? trimEnd_->text() : trimDuration_->text();
        const std::optional<double> duration = duration_ >= 0 ? std::optional<double>(duration_) : std::nullopt;
        if (!resolveTrim(mode, trimStart_->text().toStdString(), other.toStdString(), duration, error)) {
            return QString::fromUtf8(error.c_str());
        }
    }
    if (feature == Feature::Convert && convertMode_->currentIndex() == 1) {
        const auto text = bitrate_->text().trimmed();
        if (!text.isEmpty()) {
            bool ok = false;
            const int value = text.toInt(&ok);
            if (!ok || value < 1 || value > 200000) return QString::fromUtf8(kBitrateInvalid);
        }
        if (convertContainer_->currentIndex() == 2 && encoder_->currentIndex() != 2) return QString::fromUtf8(kWebmVp9Only);
    }
    if (feature == Feature::MuxSubtitle && !folder && subtitlePath_->text().trimmed().isEmpty()) return QString::fromUtf8(kPickSubtitle);
    if (feature == Feature::Audio && audioMode_->currentIndex() == 2 && !folder && audioPath_->text().trimmed().isEmpty()) {
        return QString::fromUtf8(kPickAudio);
    }
    return {};
}

void MainWindow::refresh() {
    if (refreshing_) return;
    refreshing_ = true;
    const auto feature = static_cast<MediaTool::Feature>(featureIndex());
    const bool folder = folder_->isChecked();
    single_->setText(feature == MediaTool::Feature::Merge ? "多个视频" : "单个视频");
    const bool mergeList = feature == MediaTool::Feature::Merge && !folder;
    inputStack_->setCurrentIndex(mergeList ? 1 : 0);
    inputStack_->setFixedHeight(mergeList ? 140 : path_->sizeHint().height());
    params_->setCurrentIndex(featureIndex());
    fitToCurrentPage(params_);
    paramCard_->setVisible(feature != MediaTool::Feature::Info);
    const bool remux = convertMode_->currentIndex() == 0;
    const bool webm = convertContainer_->currentIndex() == 2;
    resolution_->setEnabled(!remux);
    bitrate_->setEnabled(!remux);
    if (!remux && webm) {
        encoder_->blockSignals(true);
        encoder_->setCurrentIndex(2);
        encoder_->blockSignals(false);
        encoder_->setEnabled(false);
    } else {
        encoder_->setEnabled(!remux);
    }
    trimEnd_->setEnabled(trimEndMode_->isChecked());
    trimDuration_->setEnabled(trimDurationMode_->isChecked());
    const bool keep = audioMode_->currentIndex() == 1;
    const bool replace = audioMode_->currentIndex() == 2;
    keepTrack_->setEnabled(keep);
    editVolume_->setEnabled(keep || replace);
    audioPath_->setEnabled(replace && !folder);
    audioBrowse_->setEnabled(replace && !folder);
    audioHint_->setVisible(folder && replace);
    subtitlePath_->setEnabled(!folder);
    subtitleBrowse_->setEnabled(!folder);
    muxHint_->setVisible(folder);
    start_->setText(feature == MediaTool::Feature::Info ? "导出" : "开始");
    const QString issue = validate();
    const bool inputProblem = issue == QString::fromUtf8(MediaTool::kPickVideo) || issue == QString::fromUtf8(MediaTool::kNeedTwo);
    inputIssue_->setText(inputProblem ? issue : QString());
    inputIssue_->setVisible(!inputIssue_->text().isEmpty());
    paramIssue_->setText(inputProblem ? QString() : issue);
    paramIssue_->setVisible(!paramIssue_->text().isEmpty());
    if (!busy_) {
        start_->setEnabled(ready_ && issue.isEmpty());
        cancel_->setEnabled(false);
    }
    copyList_->setEnabled(results_->rowCount() > 0);
    const auto infoText = info_->toPlainText();
    copyInfo_->setEnabled(!infoText.isEmpty() && infoText != kFolderInfo);
    refreshing_ = false;
}

void MainWindow::setBusy(bool busy) {
    busy_ = busy;
    featureNav_->setEnabled(!busy_);
    inputCard_->setEnabled(!busy_);
    paramCard_->setEnabled(!busy_);
    about_->setEnabled(true);
    start_->setEnabled(!busy_ && ready_ && validate().isEmpty());
    cancel_->setEnabled(busy_);
}

void MainWindow::browse() {
    const auto feature = static_cast<MediaTool::Feature>(featureIndex());
    if (folder_->isChecked()) {
        const auto directory = QFileDialog::getExistingDirectory(this, "选择文件夹");
        if (directory.isEmpty()) return;
        path_->setText(directory);
    } else if (feature == MediaTool::Feature::Merge) {
        const auto picked = QFileDialog::getOpenFileNames(this, "选择视频", {}, kVideoFilter);
        for (const auto& file : picked) {
            if (files_->findItems(file, Qt::MatchExactly).isEmpty()) files_->addItem(file);
        }
        if (files_->currentRow() < 0 && files_->count() > 0) files_->setCurrentRow(0);
    } else {
        const auto file = QFileDialog::getOpenFileName(this, "选择视频", {}, kVideoFilter);
        if (file.isEmpty()) return;
        path_->setText(file);
    }
    refresh();
    syncInfo();
}

void MainWindow::probePath(const QString& path) {
    if (path.isEmpty() || !engine_) return;
    progressText_->setText("正在读取媒体信息…");
    progressPercent_->setText("");
    progress_->setRange(0, 0);
    QMetaObject::invokeMethod(engine_, [engine = engine_, path] { engine->probeFile(path); }, Qt::QueuedConnection);
}

void MainWindow::syncInfo() {
    if (busy_) return;
    if (folder_->isChecked()) {
        duration_ = -1;
        setInfoText(info_, QString::fromUtf8(kFolderInfo), true);
        refresh();
        return;
    }
    const auto feature = static_cast<MediaTool::Feature>(featureIndex());
    if (feature == MediaTool::Feature::Merge) {
        const auto* item = files_->currentItem();
        if (!item) {
            duration_ = -1;
            setInfoText(info_, QString::fromUtf8(kFolderInfo), true);
            refresh();
            return;
        }
        probePath(item->text());
        return;
    }
    if (path_->text().trimmed().isEmpty()) {
        duration_ = -1;
        setInfoText(info_, QString::fromUtf8(kFolderInfo), true);
        refresh();
        return;
    }
    probePath(path_->text());
}

MediaTool::JobRequest MainWindow::buildJob() const {
    using namespace MediaTool;
    JobRequest job;
    job.options.feature = static_cast<Feature>(featureIndex());
    job.options.audioFormat = static_cast<AudioFormat>(audioFormatGroup_->checkedId());
    job.options.subtitleFormat = static_cast<SubtitleFormat>(subFormat_->currentIndex());
    job.options.audioEdit = static_cast<AudioEditMode>(audioMode_->currentIndex());
    job.options.convertMode = static_cast<ConvertMode>(convertMode_->currentIndex());
    job.options.resolution = static_cast<ResolutionPreset>(resolution_->currentIndex());
    job.options.videoEncoder = static_cast<VideoEncoder>(encoder_->currentIndex());
    job.options.container = static_cast<ContainerKind>(job.options.feature == Feature::Merge ? mergeContainer_->currentIndex() : convertContainer_->currentIndex());
    job.options.trimMode = trimEndMode_->isChecked() ? TrimMode::StartEnd : TrimMode::StartDuration;
    job.options.trimStart = trimStart_->text().trimmed().toStdString();
    job.options.trimEnd = trimEnd_->text().trimmed().toStdString();
    job.options.trimDuration = trimDuration_->text().trimmed().toStdString();
    job.options.externalPath = (job.options.feature == Feature::MuxSubtitle ? subtitlePath_->text() : audioPath_->text()).trimmed().toStdString();
    if (job.options.feature == Feature::ExtractAudio) job.options.trackNumber = trackFrom(audioTrack_);
    else if (job.options.feature == Feature::ExtractSubtitle) job.options.trackNumber = trackFrom(subTrack_);
    else job.options.trackNumber = trackFrom(keepTrack_);
    job.options.volume = job.options.feature == Feature::ExtractAudio ? audioVolume_->value() : editVolume_->value();
    if (job.options.feature == Feature::Convert && job.options.convertMode == ConvertMode::Compress) {
        const auto text = bitrate_->text().trimmed();
        if (!text.isEmpty()) job.options.bitrateKbps = text.toInt();
    }
    if (folder_->isChecked()) job.folderPath = path_->text().toStdString();
    else if (job.options.feature == Feature::Merge) {
        for (int row = 0; row < files_->count(); ++row) job.files.push_back(files_->item(row)->text().toStdString());
    } else if (!path_->text().isEmpty()) {
        job.files.push_back(path_->text().toStdString());
    }
    return job;
}

void MainWindow::startJob() {
    if (busy_ || !ready_ || !validate().isEmpty()) return;
    results_->setRowCount(0);
    progress_->setRange(0, 0);
    progressPercent_->setText("进行中");
    setBusy(true);
    const auto job = buildJob();
    QMetaObject::invokeMethod(engine_, [engine = engine_, job] { engine->run(job); }, Qt::QueuedConnection);
}

void MainWindow::cancelJob() {
    if (!busy_ || !engine_) return;
    const auto feature = static_cast<MediaTool::Feature>(featureIndex());
    if (feature != MediaTool::Feature::Merge && folder_->isChecked()) {
        QMessageBox box(this);
        box.setWindowTitle("取消任务");
        box.setText("要停掉整个任务，还是只跳过当前文件？");
        auto* stop = box.addButton("停掉整个任务", QMessageBox::AcceptRole);
        box.addButton("只跳过当前文件", QMessageBox::ActionRole);
        box.exec();
        if (box.clickedButton() == stop) engine_->requestStop();
        else if (box.clickedButton()) engine_->requestSkip();
        return;
    }
    engine_->requestStop();
}

void MainWindow::addResult(const QString& file, const QString& status, const QString& note, const QString& output) {
    const int row = results_->rowCount();
    results_->insertRow(row);
    results_->setItem(row, 0, new QTableWidgetItem(file));
    results_->setItem(row, 1, new QTableWidgetItem(status));
    results_->setItem(row, 2, new QTableWidgetItem(note));
    results_->setItem(row, 3, new QTableWidgetItem(output));
    copyList_->setEnabled(true);
}

void MainWindow::copyInfo() {
    QGuiApplication::clipboard()->setText(info_->toPlainText());
}

void MainWindow::copyList() {
    QStringList lines;
    for (int row = 0; row < results_->rowCount(); ++row) {
        QStringList columns;
        for (int column = 0; column < results_->columnCount(); ++column) {
            const auto* item = results_->item(row, column);
            columns << (item ? item->text() : QString());
        }
        lines << columns.join('\t');
    }
    QGuiApplication::clipboard()->setText(lines.join("\r\n"));
}

void MainWindow::showAbout() {
    QDialog dialog(this);
    dialog.setWindowTitle("关于");
    dialog.setFont(uiFont());
    dialog.setMinimumWidth(360);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(10);
    layout->addWidget(new QLabel(QString("媒体小工具 %1").arg(MEDIATOOL_VERSION)));
    layout->addWidget(new QLabel(QString("FFmpeg %1").arg(ffmpegVersion_)));
    layout->addWidget(new QLabel(QString("H.264：%1").arg(h264Name_)));
    layout->addWidget(new QLabel(QString("H.265：%1").arg(h265Name_)));
    layout->addWidget(new QLabel(QString("VP9：%1").arg(vp9Name_)));
    layout->addWidget(new QLabel("本工具和随包 FFmpeg 都按 GPL 提供。"));
    auto addLicense = [&](const QString& title, const std::filesystem::path& path) {
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error)) {
            layout->addWidget(new QLabel("许可证文件缺失"));
            return;
        }
        auto* button = new QPushButton(title);
        QObject::connect(button, &QPushButton::clicked, &dialog, [path] {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdWString(path.wstring())));
        });
        layout->addWidget(button);
    };
    addLicense("本工具许可证", bundle_.licenseApp);
    addLicense("FFmpeg 许可证", bundle_.licenseFfmpeg);
    addLicense("源代码说明", bundle_.sourceNote);
    dialog.exec();
}

void MainWindow::setupTray() {
    trayMenu_ = new QMenu(this);
    trayMenu_->addAction("显示主界面", this, &MainWindow::showMainWindow);
    trayMenu_->addAction("退出", this, &MainWindow::quitFromTray);

    tray_ = new QSystemTrayIcon(this);
    const QIcon icon = windowIcon().isNull() ? QIcon(":/icons/app.png") : windowIcon();
    tray_->setIcon(icon);
    tray_->setToolTip("媒体小工具");
    tray_->setContextMenu(trayMenu_);
    connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) showMainWindow();
    });
    tray_->show();
}

void MainWindow::showMainWindow() {
    show();
    raise();
    activateWindow();
}

void MainWindow::quitFromTray() {
    if (quitting_) return;
    quitting_ = true;
    if (busy_) {
        if (engine_) engine_->requestStop();
        QTimer::singleShot(10000, this, [this] {
            if (quitting_) {
                shutdownThread();
                QApplication::quit();
            }
        });
        return;
    }
    close();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!quitting_) {
        hide();
        event->ignore();
        return;
    }
    if (busy_) {
        if (engine_) engine_->requestStop();
        QTimer::singleShot(10000, this, [this] {
            if (quitting_) {
                shutdownThread();
                QApplication::quit();
            }
        });
        event->ignore();
        return;
    }
    if (tray_) tray_->hide();
    shutdownThread();
    event->accept();
    QApplication::quit();
}

void MainWindow::shutdownThread() {
    if (!thread_) return;
    thread_->quit();
    thread_->wait(45000);
    thread_ = nullptr;
}
