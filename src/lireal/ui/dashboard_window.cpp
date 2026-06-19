/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * Lireal Music - C++ audio visual rendering engine.
 * Copyright (C) 2026 Lireal contributors
 *
 * This file is part of Lireal Music.
 * Lireal Music is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "lireal/ui/dashboard_window.hpp"

#include "lireal/render/video_renderer.hpp"
#include "lireal/system/hardware_profile.hpp"

#include "lireal/audio/audio_analyzer.hpp"

#include <QtConcurrent/QtConcurrent>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <QDragEnterEvent>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QMetaObject>
#include <QMimeData>
#include <QPointer>
#include <QPixmap>
#include <QSizePolicy>
#include <QDesktopServices>
#include <QScrollArea>
#include <QDir>
#include <QDropEvent>
#include <QUrl>
#include <QVBoxLayout>
#include <memory>
#include <sstream>
#include <thread>

namespace lireal::ui {
namespace {

QPushButton* makeBrowseButton(const QString& text) {
    auto* button = new QPushButton(text);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QDoubleSpinBox* makeDoubleSpin(double min, double max, double value, double step, int decimals = 2) {
    auto* spin = new QDoubleSpinBox;
    spin->setRange(min, max);
    spin->setValue(value);
    spin->setSingleStep(step);
    spin->setDecimals(decimals);
    return spin;
}

QSpinBox* makeSpin(int min, int max, int value, int step = 1) {
    auto* spin = new QSpinBox;
    spin->setRange(min, max);
    spin->setValue(value);
    spin->setSingleStep(step);
    return spin;
}

bool hasAllowedSuffix(const QString& path, const QStringList& suffixes) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffixes.contains(suffix);
}

QString describeSuffixes(const QStringList& suffixes) {
    QStringList display;
    for (const QString& suffix : suffixes) {
        display << QStringLiteral("*.%1").arg(suffix);
    }
    return display.join(QStringLiteral("、"));
}

QString formatDuration(qint64 elapsedMs) {
    const qint64 totalSeconds = elapsedMs / 1000;
    const qint64 minutes = totalSeconds / 60;
    const qint64 seconds = totalSeconds % 60;
    const qint64 millis = elapsedMs % 1000;
    if (minutes > 0) {
        return QStringLiteral("%1 分 %2.%3 秒").arg(minutes).arg(seconds).arg(millis, 3, 10, QChar('0'));
    }
    return QStringLiteral("%1.%2 秒").arg(seconds).arg(millis, 3, 10, QChar('0'));
}

QString formatEtaSeconds(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0.0) {
        return QStringLiteral("计算中");
    }
    const qint64 totalSeconds = static_cast<qint64>(std::round(seconds));
    const qint64 minutes = totalSeconds / 60;
    const qint64 remainSeconds = totalSeconds % 60;
    if (minutes > 0) {
        return QStringLiteral("%1 分 %2 秒").arg(minutes).arg(remainSeconds);
    }
    return QStringLiteral("%1 秒").arg(remainSeconds);
}

QString openFileDialog(QWidget* parent, const QString& title, const QString& filter) {
    return QFileDialog::getOpenFileName(parent, title, QString(), filter, nullptr, QFileDialog::DontUseNativeDialog);
}

QString saveFileDialog(QWidget* parent, const QString& title, const QString& defaultName, const QString& filter) {
    return QFileDialog::getSaveFileName(parent, title, defaultName, filter, nullptr, QFileDialog::DontUseNativeDialog);
}

} // namespace

void DashboardWindow::populateEncoderDeviceList() {
    encoderDeviceCombo_->clear();
    encoderDeviceCombo_->addItem(QStringLiteral("自动选择可用显卡"), QStringLiteral("auto"));

    const auto devices = lireal::system::detectEncoderDevices();
    for (const auto& device : devices) {
        encoderDeviceCombo_->addItem(QString::fromUtf8(device.label.c_str()), QString::fromStdString(device.device));
    }

    if (devices.empty()) {
        encoderDeviceCombo_->addItem(QStringLiteral("未检测到硬件编码显卡 · 使用 CPU/libx264"), QStringLiteral("auto"));
        encoderDeviceCombo_->model()->setData(encoderDeviceCombo_->model()->index(1, 0), 0, Qt::UserRole - 1);
        appendLog(QStringLiteral("未检测到 NVIDIA CUDA 或 VAAPI render 节点，编码显卡列表已回退到自动。"));
    } else {
        appendLog(QStringLiteral("已检测到 %1 个可用硬件编码显卡/设备。").arg(static_cast<int>(devices.size())));
    }
}

DashboardWindow::DashboardWindow(QWidget* parent)
    : QMainWindow(parent), renderWatcher_(new QFutureWatcher<void>(this)) {
    setAcceptDrops(true);
    buildUi();
    connect(renderWatcher_, &QFutureWatcher<void>::finished, this, [this]() {
        previewButton_->setEnabled(true);
        renderButton_->setEnabled(true);
        cancelButton_->setEnabled(false);
        openFolderButton_->setEnabled(!lastOutputPath_.isEmpty());
        if (renderTimer_.isValid()) {
            const QString elapsedText = formatDuration(renderTimer_.elapsed());
            appendLog(QStringLiteral("本次任务耗时：") + elapsedText);
            if (!renderHadError_ && !cancelRequested_.load()) {
                QMessageBox::information(this, QStringLiteral("渲染完成"), QStringLiteral("视频已经生成。\n耗时：%1\n输出：%2").arg(elapsedText, lastOutputPath_));
            }
        }
        if (renderWatcher_->future().isCanceled()) {
            appendLog("渲染已取消。");
        } else {
            appendLog("渲染任务结束。");
        }
    });
}

DashboardWindow::~DashboardWindow() = default;

void DashboardWindow::buildUi() {
    setWindowTitle(QStringLiteral("Lireal Dashboard · 仙侠浅色 MV 模板"));
    resize(1120, 760);

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(28, 28, 28, 28);
    rootLayout->setSpacing(18);

    auto* title = new QLabel(QStringLiteral("Lireal Dashboard"));
    title->setObjectName("HeroTitle");
    auto* subtitle = new QLabel(QStringLiteral("清冷浅色 · 仙侠国风 · 圆盘频谱 · 右侧滚动歌词 · 自动背景取色"));
    subtitle->setObjectName("HeroSubtitle");

    auto* materialBox = new QGroupBox(QStringLiteral("素材选择"));
    auto* form = new QFormLayout(materialBox);
    form->setLabelAlignment(Qt::AlignRight);

    backgroundEdit_ = new QLineEdit;
    musicEdit_ = new QLineEdit;
    lyricsEdit_ = new QLineEdit;
    outputEdit_ = new QLineEdit;
    songTitleEdit_ = new QLineEdit(QStringLiteral("出山DJ"));
    artistEdit_ = new QLineEdit(QStringLiteral("花粥"));
    watermarkEdit_ = new QLineEdit(QStringLiteral("Lireal Music"));
    const QList<QLineEdit*> pathEdits = {backgroundEdit_, musicEdit_, lyricsEdit_, outputEdit_, songTitleEdit_, artistEdit_, watermarkEdit_};
    for (QLineEdit* edit : pathEdits) {
        edit->setMinimumWidth(560);
        edit->setClearButtonEnabled(true);
        edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    backgroundEdit_->setPlaceholderText(QStringLiteral("请选择或拖入背景图片路径"));
    musicEdit_->setPlaceholderText(QStringLiteral("请选择或拖入音乐文件路径"));
    lyricsEdit_->setPlaceholderText(QStringLiteral("请选择或拖入 LRC 歌词路径"));
    outputEdit_->setPlaceholderText(QStringLiteral("请选择输出 MP4 路径"));
    songTitleEdit_->setPlaceholderText(QStringLiteral("例如：出山DJ"));
    artistEdit_->setPlaceholderText(QStringLiteral("例如：花粥"));
    watermarkEdit_->setPlaceholderText(QStringLiteral("右上角小水印，例如：Lireal Music"));

    auto addPicker = [&](const QString& label, QLineEdit* edit, QPushButton* button) {
        auto* row = new QWidget;
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(edit, 1);
        layout->addWidget(button);
        form->addRow(label, row);
    };

    auto* backgroundButton = makeBrowseButton(QStringLiteral("选择背景"));
    auto* musicButton = makeBrowseButton(QStringLiteral("选择音乐"));
    auto* lyricsButton = makeBrowseButton(QStringLiteral("选择歌词"));
    auto* outputButton = makeBrowseButton(QStringLiteral("保存位置"));

    addPicker(QStringLiteral("背景图片"), backgroundEdit_, backgroundButton);
    addPicker(QStringLiteral("音乐文件"), musicEdit_, musicButton);
    addPicker(QStringLiteral("LRC 歌词"), lyricsEdit_, lyricsButton);
    addPicker(QStringLiteral("输出 MP4"), outputEdit_, outputButton);
    form->addRow(QStringLiteral("歌曲名"), songTitleEdit_);
    form->addRow(QStringLiteral("作者"), artistEdit_);
    form->addRow(QStringLiteral("右上水印"), watermarkEdit_);

    connect(backgroundButton, &QPushButton::clicked, this, &DashboardWindow::chooseBackground);
    connect(musicButton, &QPushButton::clicked, this, &DashboardWindow::chooseMusic);
    connect(lyricsButton, &QPushButton::clicked, this, &DashboardWindow::chooseLyrics);
    connect(outputButton, &QPushButton::clicked, this, &DashboardWindow::chooseOutput);

    auto* parameterBox = new QGroupBox(QStringLiteral("画面参数"));
    auto* parameterForm = new QFormLayout(parameterBox);
    parameterForm->setLabelAlignment(Qt::AlignRight);

    qualityPresetCombo_ = new QComboBox;
    qualityPresetCombo_->addItem(QStringLiteral("快速预览 · 720p 30FPS"));
    qualityPresetCombo_->addItem(QStringLiteral("标准梦幻 · 1080p 60FPS"));
    qualityPresetCombo_->addItem(QStringLiteral("高清 2K · 1440p 60FPS"));
    qualityPresetCombo_->addItem(QStringLiteral("高能发布 · 1080p 120FPS"));
    qualityPresetCombo_->addItem(QStringLiteral("4K 超清发布 · 2160p 60FPS"));
    qualityPresetCombo_->setCurrentIndex(1);

    resolutionCombo_ = new QComboBox;
    resolutionCombo_->addItem(QStringLiteral("1920 × 1080"), QSize(1920, 1080));
    resolutionCombo_->addItem(QStringLiteral("1280 × 720"), QSize(1280, 720));
    resolutionCombo_->addItem(QStringLiteral("2560 × 1440"), QSize(2560, 1440));
    resolutionCombo_->addItem(QStringLiteral("3840 × 2160"), QSize(3840, 2160));
    resolutionCombo_->setCurrentIndex(0);

    fpsModeCombo_ = new QComboBox;
    fpsModeCombo_->addItem(QStringLiteral("标准流畅 · 60FPS"), 60);
    fpsModeCombo_->addItem(QStringLiteral("超高刷新 · 120FPS"), 120);
    fpsModeCombo_->addItem(QStringLiteral("手动 FPS"), -1);
    fpsModeCombo_->setCurrentIndex(0);
    fpsSpin_ = makeSpin(24, 120, 60, 1);
    parallaxSpin_ = makeDoubleSpin(0.0, 80.0, 26.0, 1.0, 1);
    pulseSpin_ = makeDoubleSpin(0.0, 0.20, 0.045, 0.005, 3);
    spectrumRadiusSpin_ = makeDoubleSpin(80.0, 420.0, 188.0, 4.0, 1);
    spectrumHeightSpin_ = makeDoubleSpin(20.0, 260.0, 96.0, 4.0, 1);
    glowSpin_ = makeDoubleSpin(0.0, 1.5, 0.65, 0.05, 2);
    encoderBackendCombo_ = new QComboBox;
    encoderBackendCombo_->addItem(QStringLiteral("自动选择 · 推荐"), QStringLiteral("auto"));
    encoderBackendCombo_->addItem(QStringLiteral("兼容软件 · libx264"), QStringLiteral("libx264"));
    encoderBackendCombo_->addItem(QStringLiteral("NVIDIA 硬件 · h264_nvenc"), QStringLiteral("h264_nvenc"));
    encoderBackendCombo_->addItem(QStringLiteral("Linux VAAPI 硬件 · h264_vaapi"), QStringLiteral("h264_vaapi"));
    encoderBackendCombo_->setCurrentIndex(0);
    encoderDeviceCombo_ = new QComboBox;
    populateEncoderDeviceList();
    encoderPresetCombo_ = new QComboBox;
    encoderPresetCombo_->addItem(QStringLiteral("最快 · ultrafast"), QStringLiteral("ultrafast"));
    encoderPresetCombo_->addItem(QStringLiteral("很快 · veryfast"), QStringLiteral("veryfast"));
    encoderPresetCombo_->addItem(QStringLiteral("均衡 · fast"), QStringLiteral("fast"));
    encoderPresetCombo_->addItem(QStringLiteral("高压缩 · medium"), QStringLiteral("medium"));
    encoderPresetCombo_->setCurrentIndex(1);
    encoderCrfSpin_ = makeSpin(10, 28, 17, 1);
    encoderCrfSpin_->setToolTip(QStringLiteral("数值越小画质越高、文件越大；12 适合 4K 超清，17 适合高画质，20 适合快速预览。"));
    renderThreadsSpin_ = makeSpin(0, 64, 0, 1);
    renderThreadsSpin_->setToolTip(QStringLiteral("0 表示自动并发；全局 WebGPU 渲染模式优先，缺少运行库时自动使用兼容合成，NVIDIA/VAAPI 负责硬件编码。"));
    bloomCheck_ = new QCheckBox(QStringLiteral("启用浅色 Bloom 光效"));
    bloomCheck_->setChecked(true);
    circularCoverCheck_ = new QCheckBox(QStringLiteral("启用左侧圆形封面"));
    circularCoverCheck_->setChecked(true);
    mangaFilterCheck_ = new QCheckBox(QStringLiteral("启用水墨边缘滤镜"));
    mangaFilterCheck_->setChecked(false);
    impactFlashCheck_ = new QCheckBox(QStringLiteral("启用轻微节奏冲击"));
    impactFlashCheck_->setChecked(false);
    particlesCheck_ = new QCheckBox(QStringLiteral("启用白色雪点粒子"));
    particlesCheck_->setChecked(true);

    lyricLayoutCombo_ = new QComboBox;
    lyricLayoutCombo_->addItem(QStringLiteral("右侧歌词队列"));
    lyricLayoutCombo_->addItem(QStringLiteral("中央大字歌词"));
    lyricLayoutCombo_->addItem(QStringLiteral("底部卡拉 OK"));

    previewTimeCombo_ = new QComboBox;
    previewTimeCombo_->addItem(QStringLiteral("5 秒"), 5.0);
    previewTimeCombo_->addItem(QStringLiteral("15 秒"), 15.0);
    previewTimeCombo_->addItem(QStringLiteral("30 秒"), 30.0);
    previewTimeCombo_->addItem(QStringLiteral("自定义秒数"), -1.0);
    previewTimeCombo_->setCurrentIndex(1);
    previewTimeSpin_ = makeDoubleSpin(0.0, 3600.0, 15.0, 1.0, 1);

    parameterForm->addRow(QStringLiteral("质量预设"), qualityPresetCombo_);
    parameterForm->addRow(QStringLiteral("分辨率"), resolutionCombo_);
    parameterForm->addRow(QStringLiteral("帧率模式"), fpsModeCombo_);
    parameterForm->addRow(QStringLiteral("帧率 FPS"), fpsSpin_);
    parameterForm->addRow(QStringLiteral("视差强度"), parallaxSpin_);
    parameterForm->addRow(QStringLiteral("呼吸强度"), pulseSpin_);
    parameterForm->addRow(QStringLiteral("频谱半径"), spectrumRadiusSpin_);
    parameterForm->addRow(QStringLiteral("频谱高度"), spectrumHeightSpin_);
    parameterForm->addRow(QStringLiteral("Glow 强度"), glowSpin_);
    parameterForm->addRow(QStringLiteral("编码后端"), encoderBackendCombo_);
    parameterForm->addRow(QStringLiteral("编码显卡"), encoderDeviceCombo_);
    parameterForm->addRow(QStringLiteral("编码速度"), encoderPresetCombo_);
    parameterForm->addRow(QStringLiteral("编码 CRF"), encoderCrfSpin_);
    parameterForm->addRow(QStringLiteral("合成线程"), renderThreadsSpin_);
    parameterForm->addRow(QStringLiteral("Bloom"), bloomCheck_);
    parameterForm->addRow(QStringLiteral("圆形封面"), circularCoverCheck_);
    parameterForm->addRow(QStringLiteral("漫画滤镜"), mangaFilterCheck_);
    parameterForm->addRow(QStringLiteral("闪白冲击"), impactFlashCheck_);
    parameterForm->addRow(QStringLiteral("碎片粒子"), particlesCheck_);
    parameterForm->addRow(QStringLiteral("歌词布局"), lyricLayoutCombo_);
    parameterForm->addRow(QStringLiteral("预览时间点"), previewTimeCombo_);
    parameterForm->addRow(QStringLiteral("自定义秒数"), previewTimeSpin_);

    connect(qualityPresetCombo_, &QComboBox::currentIndexChanged, this, &DashboardWindow::applyQualityPreset);
    connect(fpsModeCombo_, &QComboBox::currentIndexChanged, this, [this]() {
        const int fps = fpsModeCombo_->currentData().toInt();
        fpsSpin_->setEnabled(fps < 0);
        if (fps > 0) {
            fpsSpin_->setValue(fps);
        }
    });
    connect(previewTimeCombo_, &QComboBox::currentIndexChanged, this, [this]() {
        previewTimeSpin_->setEnabled(previewTimeCombo_->currentData().toDouble() < 0.0);
    });
    connect(encoderDeviceCombo_, &QComboBox::currentIndexChanged, this, [this]() {
        const QString device = encoderDeviceCombo_->currentData().toString();
        if (device.startsWith(QStringLiteral("cuda:"))) {
            const int index = encoderBackendCombo_->findData(QStringLiteral("h264_nvenc"));
            if (index >= 0) {
                encoderBackendCombo_->setCurrentIndex(index);
            }
        } else if (device.startsWith(QStringLiteral("vaapi:"))) {
            const int index = encoderBackendCombo_->findData(QStringLiteral("h264_vaapi"));
            if (index >= 0) {
                encoderBackendCombo_->setCurrentIndex(index);
            }
        }
    });
    previewTimeSpin_->setEnabled(false);

    auto* actionBox = new QGroupBox(QStringLiteral("渲染状态"));
    auto* actionLayout = new QVBoxLayout(actionBox);
    progressBar_ = new QProgressBar;
    progressBar_->setRange(0, 1000);
    progressBar_->setValue(0);
    renderStatsLabel_ = new QLabel(QStringLiteral("状态：等待开始 · 速度：-- FPS · 剩余：--"));
    renderStatsLabel_->setObjectName("RenderStatsLabel");
    previewButton_ = new QPushButton(QStringLiteral("打开实时预览窗口"));
    previewButton_->setCursor(Qt::PointingHandCursor);
    renderButton_ = new QPushButton(QStringLiteral("开始生成仙侠浅色 MV"));
    renderButton_->setObjectName("PrimaryButton");
    renderButton_->setCursor(Qt::PointingHandCursor);
    cancelButton_ = new QPushButton(QStringLiteral("取消当前渲染"));
    cancelButton_->setCursor(Qt::PointingHandCursor);
    cancelButton_->setEnabled(false);
    openFolderButton_ = new QPushButton(QStringLiteral("打开输出文件夹"));
    openFolderButton_->setCursor(Qt::PointingHandCursor);
    openFolderButton_->setEnabled(false);
    savePresetButton_ = new QPushButton(QStringLiteral("保存风格预设"));
    savePresetButton_->setCursor(Qt::PointingHandCursor);
    loadPresetButton_ = new QPushButton(QStringLiteral("加载风格预设"));
    loadPresetButton_->setCursor(Qt::PointingHandCursor);
    logView_ = new QTextEdit;
    logView_->setReadOnly(true);
    logView_->setMinimumHeight(220);

    actionLayout->addWidget(progressBar_);
    actionLayout->addWidget(renderStatsLabel_);
    actionLayout->addWidget(previewButton_);
    actionLayout->addWidget(renderButton_);
    actionLayout->addWidget(cancelButton_);
    actionLayout->addWidget(openFolderButton_);
    auto* presetRow = new QWidget;
    auto* presetLayout = new QHBoxLayout(presetRow);
    presetLayout->setContentsMargins(0, 0, 0, 0);
    presetLayout->addWidget(savePresetButton_);
    presetLayout->addWidget(loadPresetButton_);
    actionLayout->addWidget(presetRow);
    actionLayout->addWidget(logView_, 1);
    connect(previewButton_, &QPushButton::clicked, this, &DashboardWindow::generatePreview);
    connect(renderButton_, &QPushButton::clicked, this, &DashboardWindow::startRender);
    connect(cancelButton_, &QPushButton::clicked, this, &DashboardWindow::cancelRender);
    connect(openFolderButton_, &QPushButton::clicked, this, &DashboardWindow::openOutputFolder);
    connect(savePresetButton_, &QPushButton::clicked, this, &DashboardWindow::saveStylePreset);
    connect(loadPresetButton_, &QPushButton::clicked, this, &DashboardWindow::loadStylePreset);

    rootLayout->addWidget(title);
    rootLayout->addWidget(subtitle);
    auto* scrollContent = new QWidget;
    auto* scrollLayout = new QHBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(18);
    auto* leftColumn = new QWidget;
    auto* leftLayout = new QVBoxLayout(leftColumn);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(18);
    leftLayout->addWidget(materialBox);
    leftLayout->addWidget(parameterBox);
    leftLayout->addStretch(1);
    actionBox->setMinimumWidth(360);
    scrollLayout->addWidget(leftColumn, 7);
    scrollLayout->addWidget(actionBox, 3);
    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidget(scrollContent);

    rootLayout->addWidget(scrollArea, 1);

    setCentralWidget(central);
    appendLog(QStringLiteral("欢迎使用 Lireal Dashboard，请先选择背景、音乐和歌词。"));
    appendLog(QStringLiteral("模板：左侧歌名/作者/圆盘频谱，右侧滚动歌词，配色自动跟随背景浅色化。"));
}

void DashboardWindow::chooseBackground() {
    const QString path = openFileDialog(this, QStringLiteral("选择背景图片"), QStringLiteral("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
    if (!path.isEmpty()) {
        backgroundEdit_->setText(path);
    }
}

void DashboardWindow::chooseMusic() {
    const QString path = openFileDialog(this, QStringLiteral("选择音乐文件"), QStringLiteral("Audio (*.mp3 *.wav *.flac *.aac *.ogg *.m4a)"));
    if (!path.isEmpty()) {
        musicEdit_->setText(path);
    }
}

void DashboardWindow::chooseLyrics() {
    const QString path = openFileDialog(this, QStringLiteral("选择 LRC 歌词"), QStringLiteral("LRC lyrics (*.lrc);;Text files (*.txt)"));
    if (!path.isEmpty()) {
        lyricsEdit_->setText(path);
    }
}

void DashboardWindow::chooseOutput() {
    const QString path = saveFileDialog(this, QStringLiteral("选择输出视频"), QStringLiteral("lireal_output.mp4"), QStringLiteral("MP4 Video (*.mp4)"));
    if (!path.isEmpty()) {
        QString fixedPath = path;
        if (!fixedPath.endsWith(QStringLiteral(".mp4"), Qt::CaseInsensitive)) {
            fixedPath += QStringLiteral(".mp4");
        }
        outputEdit_->setText(fixedPath);
    }
}

void DashboardWindow::saveStylePreset() {
    const QString path = saveFileDialog(this, QStringLiteral("保存风格预设"), QStringLiteral("lireal_style.lirealstyle.json"), QStringLiteral("Lireal Style (*.lirealstyle.json);;JSON (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    QJsonObject root;
    root.insert(QStringLiteral("schema"), QStringLiteral("lireal-style-v1"));
    root.insert(QStringLiteral("qualityPreset"), qualityPresetCombo_->currentIndex());
    root.insert(QStringLiteral("songTitle"), songTitleEdit_->text());
    root.insert(QStringLiteral("artistName"), artistEdit_->text());
    root.insert(QStringLiteral("watermarkText"), watermarkEdit_->text());
    root.insert(QStringLiteral("resolution"), resolutionCombo_->currentIndex());
    root.insert(QStringLiteral("fpsMode"), fpsModeCombo_->currentIndex());
    root.insert(QStringLiteral("fps"), fpsSpin_->value());
    root.insert(QStringLiteral("parallaxStrength"), parallaxSpin_->value());
    root.insert(QStringLiteral("pulseStrength"), pulseSpin_->value());
    root.insert(QStringLiteral("spectrumRadius"), spectrumRadiusSpin_->value());
    root.insert(QStringLiteral("spectrumBarHeight"), spectrumHeightSpin_->value());
    root.insert(QStringLiteral("glowStrength"), glowSpin_->value());
    root.insert(QStringLiteral("encoderBackend"), encoderBackendCombo_->currentData().toString());
    root.insert(QStringLiteral("encoderDevice"), encoderDeviceCombo_->currentData().toString());
    root.insert(QStringLiteral("encoderPreset"), encoderPresetCombo_->currentData().toString());
    root.insert(QStringLiteral("encoderCrf"), encoderCrfSpin_->value());
    root.insert(QStringLiteral("renderThreads"), renderThreadsSpin_->value());
    root.insert(QStringLiteral("lyricLayoutMode"), lyricLayoutCombo_->currentIndex());
    root.insert(QStringLiteral("previewTimeMode"), previewTimeCombo_->currentIndex());
    root.insert(QStringLiteral("previewTimeSeconds"), previewTimeSpin_->value());
    root.insert(QStringLiteral("enableBloom"), bloomCheck_->isChecked());
    root.insert(QStringLiteral("enableCircularCover"), circularCoverCheck_->isChecked());
    root.insert(QStringLiteral("enableMangaFilter"), mangaFilterCheck_->isChecked());
    root.insert(QStringLiteral("enableImpactFlash"), impactFlashCheck_->isChecked());
    root.insert(QStringLiteral("enableParticles"), particlesCheck_->isChecked());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("无法写入风格预设：\n%1").arg(path));
        return;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    appendLog(QStringLiteral("风格预设已保存：") + path);
}

void DashboardWindow::loadStylePreset() {
    const QString path = openFileDialog(this, QStringLiteral("加载风格预设"), QStringLiteral("Lireal Style (*.lirealstyle.json *.json);;JSON (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("加载失败"), QStringLiteral("无法读取风格预设：\n%1").arg(path));
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        QMessageBox::warning(this, QStringLiteral("加载失败"), QStringLiteral("风格预设不是有效 JSON 对象。"));
        return;
    }

    const QJsonObject root = document.object();
    qualityPresetCombo_->blockSignals(true);
    qualityPresetCombo_->setCurrentIndex(std::clamp(root.value(QStringLiteral("qualityPreset")).toInt(qualityPresetCombo_->currentIndex()), 0, qualityPresetCombo_->count() - 1));
    qualityPresetCombo_->blockSignals(false);
    songTitleEdit_->setText(root.value(QStringLiteral("songTitle")).toString(songTitleEdit_->text()));
    artistEdit_->setText(root.value(QStringLiteral("artistName")).toString(artistEdit_->text()));
    watermarkEdit_->setText(root.value(QStringLiteral("watermarkText")).toString(watermarkEdit_->text()));
    resolutionCombo_->setCurrentIndex(std::clamp(root.value(QStringLiteral("resolution")).toInt(resolutionCombo_->currentIndex()), 0, resolutionCombo_->count() - 1));
    fpsModeCombo_->setCurrentIndex(std::clamp(root.value(QStringLiteral("fpsMode")).toInt(fpsModeCombo_->currentIndex()), 0, fpsModeCombo_->count() - 1));
    fpsSpin_->setValue(root.value(QStringLiteral("fps")).toInt(fpsSpin_->value()));
    fpsSpin_->setEnabled(fpsModeCombo_->currentData().toInt() < 0);
    parallaxSpin_->setValue(root.value(QStringLiteral("parallaxStrength")).toDouble(parallaxSpin_->value()));
    pulseSpin_->setValue(root.value(QStringLiteral("pulseStrength")).toDouble(pulseSpin_->value()));
    spectrumRadiusSpin_->setValue(root.value(QStringLiteral("spectrumRadius")).toDouble(spectrumRadiusSpin_->value()));
    spectrumHeightSpin_->setValue(root.value(QStringLiteral("spectrumBarHeight")).toDouble(spectrumHeightSpin_->value()));
    glowSpin_->setValue(root.value(QStringLiteral("glowStrength")).toDouble(glowSpin_->value()));
    const QString encoderBackend = root.value(QStringLiteral("encoderBackend")).toString(encoderBackendCombo_->currentData().toString());
    const int backendIndex = encoderBackendCombo_->findData(encoderBackend);
    if (backendIndex >= 0) {
        encoderBackendCombo_->setCurrentIndex(backendIndex);
    }
    const QString encoderDevice = root.value(QStringLiteral("encoderDevice")).toString(encoderDeviceCombo_->currentData().toString());
    const int deviceIndex = encoderDeviceCombo_->findData(encoderDevice);
    if (deviceIndex >= 0) {
        encoderDeviceCombo_->setCurrentIndex(deviceIndex);
    }
    const QString encoderPreset = root.value(QStringLiteral("encoderPreset")).toString(encoderPresetCombo_->currentData().toString());
    const int presetIndex = encoderPresetCombo_->findData(encoderPreset);
    if (presetIndex >= 0) {
        encoderPresetCombo_->setCurrentIndex(presetIndex);
    }
    encoderCrfSpin_->setValue(root.value(QStringLiteral("encoderCrf")).toInt(encoderCrfSpin_->value()));
    renderThreadsSpin_->setValue(root.value(QStringLiteral("renderThreads")).toInt(renderThreadsSpin_->value()));
    lyricLayoutCombo_->setCurrentIndex(std::clamp(root.value(QStringLiteral("lyricLayoutMode")).toInt(lyricLayoutCombo_->currentIndex()), 0, lyricLayoutCombo_->count() - 1));
    previewTimeCombo_->setCurrentIndex(std::clamp(root.value(QStringLiteral("previewTimeMode")).toInt(previewTimeCombo_->currentIndex()), 0, previewTimeCombo_->count() - 1));
    previewTimeSpin_->setValue(root.value(QStringLiteral("previewTimeSeconds")).toDouble(previewTimeSpin_->value()));
    previewTimeSpin_->setEnabled(previewTimeCombo_->currentData().toDouble() < 0.0);
    bloomCheck_->setChecked(root.value(QStringLiteral("enableBloom")).toBool(bloomCheck_->isChecked()));
    circularCoverCheck_->setChecked(root.value(QStringLiteral("enableCircularCover")).toBool(circularCoverCheck_->isChecked()));
    mangaFilterCheck_->setChecked(root.value(QStringLiteral("enableMangaFilter")).toBool(mangaFilterCheck_->isChecked()));
    impactFlashCheck_->setChecked(root.value(QStringLiteral("enableImpactFlash")).toBool(impactFlashCheck_->isChecked()));
    particlesCheck_->setChecked(root.value(QStringLiteral("enableParticles")).toBool(particlesCheck_->isChecked()));
    appendLog(QStringLiteral("风格预设已加载：") + path);
}

void DashboardWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (!event->mimeData()->hasUrls()) {
        return;
    }
    for (const QUrl& url : event->mimeData()->urls()) {
        const QString path = url.toLocalFile();
        if (hasAllowedSuffix(path, {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("webp"), QStringLiteral("bmp"), QStringLiteral("mp3"), QStringLiteral("wav"), QStringLiteral("flac"), QStringLiteral("aac"), QStringLiteral("ogg"), QStringLiteral("m4a"), QStringLiteral("lrc"), QStringLiteral("txt"), QStringLiteral("mp4")})) {
            event->acceptProposedAction();
            return;
        }
    }
}

void DashboardWindow::dropEvent(QDropEvent* event) {
    const QStringList imageSuffixes = {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("webp"), QStringLiteral("bmp")};
    const QStringList audioSuffixes = {QStringLiteral("mp3"), QStringLiteral("wav"), QStringLiteral("flac"), QStringLiteral("aac"), QStringLiteral("ogg"), QStringLiteral("m4a")};
    const QStringList lyricSuffixes = {QStringLiteral("lrc"), QStringLiteral("txt")};

    bool assigned = false;
    for (const QUrl& url : event->mimeData()->urls()) {
        const QString path = url.toLocalFile();
        if (path.isEmpty()) {
            continue;
        }
        if (hasAllowedSuffix(path, imageSuffixes)) {
            backgroundEdit_->setText(path);
            appendLog(QStringLiteral("已拖拽导入背景：") + path);
            assigned = true;
        } else if (hasAllowedSuffix(path, audioSuffixes)) {
            musicEdit_->setText(path);
            appendLog(QStringLiteral("已拖拽导入音乐：") + path);
            assigned = true;
        } else if (hasAllowedSuffix(path, lyricSuffixes)) {
            lyricsEdit_->setText(path);
            appendLog(QStringLiteral("已拖拽导入歌词：") + path);
            assigned = true;
        } else if (hasAllowedSuffix(path, {QStringLiteral("mp4")})) {
            outputEdit_->setText(path);
            appendLog(QStringLiteral("已拖拽设置输出：") + path);
            assigned = true;
        }
    }

    if (assigned) {
        event->acceptProposedAction();
    }
}

void DashboardWindow::startRender() {
    auto config = collectConfig();
    if (!validateConfig(config)) {
        return;
    }

    renderButton_->setEnabled(false);
    previewButton_->setEnabled(false);
    cancelButton_->setEnabled(true);
    openFolderButton_->setEnabled(false);
    cancelRequested_.store(false);
    renderHadError_ = false;
    lastOutputPath_ = QString::fromStdString(config.outputVideoPath.string());
    progressBar_->setValue(0);
    renderStatsLabel_->setText(QStringLiteral("状态：准备中 · 速度：-- FPS · 剩余：计算中"));
    lastProgressUiUpdateMs_ = -1;
    lastLoggedProgressPercent_ = -1;
    renderTimer_.restart();
    appendLog(QStringLiteral("开始渲染：%1").arg(QString::fromStdString(config.outputVideoPath.string())));
    appendLog(QStringLiteral("录制模式：已启用预览窗口，编码器直接录制同一份 %1×%2 原始合成帧，不走屏幕截图压缩。").arg(config.width).arg(config.height));
    appendLog(QStringLiteral("性能模式：合成后端=%1，并发线程=%2；NVENC/VAAPI 只加速 H.264 编码，画面合成需 WebGPU/Dawn 后端或 CPU/OpenMP。")
        .arg(QString::fromStdString(config.renderBackend))
        .arg(config.renderThreads <= 0 ? QStringLiteral("自动") : QString::number(config.renderThreads)));

    auto* recordDialog = new QDialog(this);
    recordDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    recordDialog->setWindowTitle(QStringLiteral("Lireal 录制预览 · 编码同源帧"));
    recordDialog->resize(std::min(config.width + 360, 1760), std::min(config.height + 70, 940));
    auto* recordLayout = new QVBoxLayout(recordDialog);
    auto* recordBodyLayout = new QHBoxLayout;
    auto* recordLabel = new QLabel(QStringLiteral("正在启动录制预览…"));
    recordLabel->setAlignment(Qt::AlignCenter);
    recordLabel->setMinimumSize(640, 360);
    recordLabel->setStyleSheet(QStringLiteral("background:#eff8ff;color:#244760;border-radius:18px;"));
    auto* recordAudioPanel = new QTextEdit;
    recordAudioPanel->setReadOnly(true);
    recordAudioPanel->setMinimumWidth(320);
    recordAudioPanel->setStyleSheet(QStringLiteral("background:#fff7fc;color:#5d4260;border:1px solid #ffd5ec;border-radius:16px;padding:12px;font-family:'Noto Sans Mono CJK SC','monospace';"));
    recordAudioPanel->setText(QStringLiteral("🎧 音频处理状态\n\n等待编码预览帧喵…"));
    auto* recordHintLabel = new QLabel(QStringLiteral("窗口用于实时查看；最终 MP4 由同源原始帧直接编码，4K 源不会被窗口大小压缩。"));
    recordHintLabel->setAlignment(Qt::AlignCenter);
    recordBodyLayout->addWidget(recordLabel, 1);
    recordBodyLayout->addWidget(recordAudioPanel);
    recordLayout->addLayout(recordBodyLayout, 1);
    recordLayout->addWidget(recordHintLabel);
    recordDialog->show();
    QPointer<QDialog> recordDialogGuard(recordDialog);
    QPointer<QLabel> recordLabelGuard(recordLabel);
    QPointer<QTextEdit> recordAudioPanelGuard(recordAudioPanel);

    auto future = QtConcurrent::run([this, config, recordDialogGuard, recordLabelGuard, recordAudioPanelGuard]() {
        try {
            lireal::render::VideoRenderer renderer;
            renderer.render(config, [this](const lireal::render::RenderProgress& progress) {
                QMetaObject::invokeMethod(this, [this, progress]() {
                    updateRenderStats(progress);
                }, Qt::QueuedConnection);
            }, [this]() {
                return cancelRequested_.load();
            }, [this, recordDialogGuard, recordLabelGuard, recordAudioPanelGuard](const QImage& image, int current, int total, const lireal::audio::AudioFrameEnergy& energy) {
                QMetaObject::invokeMethod(this, [recordDialogGuard, recordLabelGuard, recordAudioPanelGuard, image, current, total, energy]() {
                    if (recordDialogGuard.isNull() || recordLabelGuard.isNull()) {
                        return;
                    }
                    recordLabelGuard->setPixmap(QPixmap::fromImage(image).scaled(recordLabelGuard->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    recordLabelGuard->setToolTip(QStringLiteral("录制帧 %1/%2 · 源分辨率 %3×%4").arg(current).arg(total).arg(image.width()).arg(image.height()));
                    if (!recordAudioPanelGuard.isNull()) {
                        recordAudioPanelGuard->setText(describeAudioEnergy(energy, current, total));
                    }
                }, Qt::QueuedConnection);
            });
        } catch (const std::exception& error) {
            QMetaObject::invokeMethod(this, [this, message = QString::fromUtf8(error.what())]() {
                if (message.contains(QStringLiteral("取消"))) {
                    appendLog(message);
                } else {
                    renderHadError_ = true;
                    QMessageBox::critical(this, QStringLiteral("渲染失败"), message);
                    appendLog(QStringLiteral("错误：") + message);
                }
            }, Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(this, [recordDialogGuard, recordLabelGuard]() {
            if (!recordDialogGuard.isNull() && !recordLabelGuard.isNull()) {
                recordLabelGuard->setText(QStringLiteral("录制预览结束"));
            }
        }, Qt::QueuedConnection);
    });
    renderWatcher_->setFuture(future);
}

void DashboardWindow::updateRenderStats(const lireal::render::RenderProgress& progress) {
    const qint64 elapsedMs = std::max<qint64>(1, renderTimer_.elapsed());
    const double clampedProgress = std::clamp(progress.progress, 0.0, 1.0);
    const int progressValue = static_cast<int>(clampedProgress * 1000.0);
    progressBar_->setValue(progressValue);

    const double elapsedSeconds = static_cast<double>(elapsedMs) / 1000.0;
    const double renderFps = progress.currentFrame > 0 ? static_cast<double>(progress.currentFrame) / elapsedSeconds : 0.0;
    const int remainingFrames = std::max(0, progress.totalFrames - progress.currentFrame);
    const double etaSeconds = renderFps > 0.01 ? static_cast<double>(remainingFrames) / renderFps : -1.0;
    const int percent = static_cast<int>(std::round(clampedProgress * 100.0));

    const bool shouldRefreshStats = lastProgressUiUpdateMs_ < 0 || elapsedMs - lastProgressUiUpdateMs_ >= 350 || progress.currentFrame >= progress.totalFrames;
    if (shouldRefreshStats) {
        lastProgressUiUpdateMs_ = elapsedMs;
        renderStatsLabel_->setText(QStringLiteral("状态：%1 · %2% · 速度：%3 FPS · 剩余：%4 · 帧：%5/%6")
            .arg(QString::fromStdString(progress.message))
            .arg(percent)
            .arg(renderFps, 0, 'f', 1)
            .arg(formatEtaSeconds(etaSeconds))
            .arg(progress.currentFrame)
            .arg(progress.totalFrames));
    }

    if (percent >= lastLoggedProgressPercent_ + 5 || progress.currentFrame >= progress.totalFrames) {
        lastLoggedProgressPercent_ = percent;
        appendLog(QStringLiteral("%1：%2% · %3/%4 · 速度 %5 FPS · 剩余 %6")
            .arg(QString::fromStdString(progress.message))
            .arg(percent)
            .arg(progress.currentFrame)
            .arg(progress.totalFrames)
            .arg(renderFps, 0, 'f', 1)
            .arg(formatEtaSeconds(etaSeconds)));
    }
}

void DashboardWindow::cancelRender() {
    cancelRequested_.store(true);
    cancelButton_->setEnabled(false);
    appendLog(QStringLiteral("已请求取消，正在等待当前帧安全结束。"));
}

void DashboardWindow::openOutputFolder() {
    QString folderPath;
    if (!lastOutputPath_.isEmpty()) {
        folderPath = QFileInfo(lastOutputPath_).dir().absolutePath();
    } else if (!outputEdit_->text().trimmed().isEmpty()) {
        folderPath = QFileInfo(outputEdit_->text().trimmed()).dir().absolutePath();
    }

    if (folderPath.isEmpty() || !QDir(folderPath).exists()) {
        QMessageBox::warning(this, QStringLiteral("无法打开文件夹"), QStringLiteral("输出文件夹不存在。"));
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
}

void DashboardWindow::applyQualityPreset() {
    switch (qualityPresetCombo_->currentIndex()) {
    case 0:
        resolutionCombo_->setCurrentIndex(1);
        fpsSpin_->setValue(30);
        parallaxSpin_->setValue(18.0);
        pulseSpin_->setValue(0.032);
        spectrumRadiusSpin_->setValue(152.0);
        spectrumHeightSpin_->setValue(72.0);
        glowSpin_->setValue(0.42);
        encoderBackendCombo_->setCurrentIndex(0);
        encoderDeviceCombo_->setCurrentIndex(0);
        encoderPresetCombo_->setCurrentIndex(0);
        encoderCrfSpin_->setValue(20);
        renderThreadsSpin_->setValue(0);
        bloomCheck_->setChecked(true);
        mangaFilterCheck_->setChecked(false);
        particlesCheck_->setChecked(false);
        break;
    case 1:
        resolutionCombo_->setCurrentIndex(0);
        fpsModeCombo_->setCurrentIndex(0);
        fpsSpin_->setValue(60);
        parallaxSpin_->setValue(26.0);
        pulseSpin_->setValue(0.045);
        spectrumRadiusSpin_->setValue(188.0);
        spectrumHeightSpin_->setValue(96.0);
        glowSpin_->setValue(0.65);
        encoderBackendCombo_->setCurrentIndex(0);
        encoderDeviceCombo_->setCurrentIndex(0);
        encoderPresetCombo_->setCurrentIndex(1);
        encoderCrfSpin_->setValue(17);
        renderThreadsSpin_->setValue(0);
        bloomCheck_->setChecked(true);
        mangaFilterCheck_->setChecked(true);
        particlesCheck_->setChecked(true);
        break;
    case 2:
        resolutionCombo_->setCurrentIndex(2);
        fpsModeCombo_->setCurrentIndex(0);
        fpsSpin_->setValue(60);
        parallaxSpin_->setValue(30.0);
        pulseSpin_->setValue(0.04);
        spectrumRadiusSpin_->setValue(236.0);
        spectrumHeightSpin_->setValue(116.0);
        glowSpin_->setValue(0.78);
        encoderBackendCombo_->setCurrentIndex(0);
        encoderDeviceCombo_->setCurrentIndex(0);
        encoderPresetCombo_->setCurrentIndex(1);
        encoderCrfSpin_->setValue(16);
        renderThreadsSpin_->setValue(0);
        bloomCheck_->setChecked(true);
        mangaFilterCheck_->setChecked(true);
        particlesCheck_->setChecked(true);
        break;
    case 3:
        resolutionCombo_->setCurrentIndex(0);
        fpsModeCombo_->setCurrentIndex(1);
        fpsSpin_->setValue(120);
        parallaxSpin_->setValue(38.0);
        pulseSpin_->setValue(0.068);
        spectrumRadiusSpin_->setValue(204.0);
        spectrumHeightSpin_->setValue(138.0);
        glowSpin_->setValue(0.9);
        encoderBackendCombo_->setCurrentIndex(0);
        encoderDeviceCombo_->setCurrentIndex(0);
        encoderPresetCombo_->setCurrentIndex(2);
        encoderCrfSpin_->setValue(15);
        renderThreadsSpin_->setValue(0);
        bloomCheck_->setChecked(true);
        mangaFilterCheck_->setChecked(true);
        particlesCheck_->setChecked(true);
        impactFlashCheck_->setChecked(true);
        break;
    case 4:
        resolutionCombo_->setCurrentIndex(3);
        fpsModeCombo_->setCurrentIndex(0);
        fpsSpin_->setValue(60);
        parallaxSpin_->setValue(36.0);
        pulseSpin_->setValue(0.052);
        spectrumRadiusSpin_->setValue(300.0);
        spectrumHeightSpin_->setValue(176.0);
        glowSpin_->setValue(0.82);
        encoderBackendCombo_->setCurrentIndex(2);
        encoderDeviceCombo_->setCurrentIndex(3);
        encoderPresetCombo_->setCurrentIndex(2);
        encoderCrfSpin_->setValue(12);
        renderThreadsSpin_->setValue(0);
        bloomCheck_->setChecked(true);
        mangaFilterCheck_->setChecked(false);
        particlesCheck_->setChecked(true);
        impactFlashCheck_->setChecked(false);
        break;
    default:
        break;
    }
    appendLog(QStringLiteral("已应用质量预设：") + qualityPresetCombo_->currentText());
}

void DashboardWindow::generatePreview() {
    auto config = collectConfig();
    if (!validateConfig(config)) {
        return;
    }

    const double selectedPreviewTime = previewTimeCombo_->currentData().toDouble();
    const double previewTimeSeconds = selectedPreviewTime < 0.0 ? previewTimeSpin_->value() : selectedPreviewTime;
    constexpr double previewDurationSeconds = 12.0;

    auto* previewDialog = new QDialog(this);
    previewDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    previewDialog->setWindowTitle(QStringLiteral("Lireal 实时预览 · 原始分辨率合成 / 窗口缩放显示"));
    previewDialog->resize(std::min(config.width + 360, 1760), std::min(config.height + 70, 940));
    auto* previewLayout = new QVBoxLayout(previewDialog);
    auto* previewBodyLayout = new QHBoxLayout;
    auto* previewLabel = new QLabel(QStringLiteral("正在分析音乐并准备实时预览…"));
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setMinimumSize(640, 360);
    previewLabel->setStyleSheet(QStringLiteral("background:#140f1f;color:#fff0fb;border-radius:18px;"));
    auto* audioPanel = new QTextEdit;
    audioPanel->setReadOnly(true);
    audioPanel->setMinimumWidth(320);
    audioPanel->setStyleSheet(QStringLiteral("background:#fff7fc;color:#5d4260;border:1px solid #ffd5ec;border-radius:16px;padding:12px;font-family:'Noto Sans Mono CJK SC','monospace';"));
    audioPanel->setText(QStringLiteral("🎧 音频处理状态\n\n正在解码、分轨和计算频谱喵…"));
    auto* hintLabel = new QLabel(QStringLiteral("预览窗口只缩放显示；实际合成与录制源帧保持所选分辨率，4K 不再降采样。"));
    hintLabel->setAlignment(Qt::AlignCenter);
    previewBodyLayout->addWidget(previewLabel, 1);
    previewBodyLayout->addWidget(audioPanel);
    previewLayout->addLayout(previewBodyLayout, 1);
    previewLayout->addWidget(hintLabel);
    previewDialog->show();

    QPointer<QDialog> dialogGuard(previewDialog);
    QPointer<QLabel> labelGuard(previewLabel);
    QPointer<QTextEdit> audioPanelGuard(audioPanel);
    auto cancelPreview = std::make_shared<std::atomic_bool>(false);
    connect(previewDialog, &QObject::destroyed, this, [cancelPreview]() {
        cancelPreview->store(true);
    });

    previewButton_->setEnabled(false);
    renderButton_->setEnabled(false);
    cancelButton_->setEnabled(false);
    progressBar_->setValue(0);
    renderTimer_.restart();
    appendLog(QStringLiteral("打开极速实时预览：从 %1 秒开始播放 %2 秒，导出源为 %3×%4，预览会自动降采样到最多 1280×720 / 独立 30FPS。")
        .arg(previewTimeSeconds, 0, 'f', 1)
        .arg(previewDurationSeconds, 0, 'f', 1)
        .arg(config.width)
        .arg(config.height));

    auto future = QtConcurrent::run([this, config, previewTimeSeconds, dialogGuard, labelGuard, audioPanelGuard, cancelPreview]() {
        try {
            lireal::render::VideoRenderer renderer;
            renderer.renderPreviewStream(config, previewTimeSeconds, 12.0, [this, dialogGuard, labelGuard, audioPanelGuard, cancelPreview, previewFps = std::clamp(config.previewFps, 12, 60)](const QImage& image, int current, int total, const lireal::audio::AudioFrameEnergy& energy) {
                if (cancelPreview->load()) {
                    return;
                }
                QMetaObject::invokeMethod(this, [this, dialogGuard, labelGuard, audioPanelGuard, image, current, total, energy]() {
                    if (dialogGuard.isNull() || labelGuard.isNull()) {
                        return;
                    }
                    labelGuard->setPixmap(QPixmap::fromImage(image).scaled(labelGuard->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    if (!audioPanelGuard.isNull()) {
                        audioPanelGuard->setText(describeAudioEnergy(energy, current, total));
                    }
                    progressBar_->setValue(static_cast<int>((static_cast<double>(current) / static_cast<double>(total)) * 1000.0));
                }, Qt::QueuedConnection);
                std::this_thread::sleep_for(std::chrono::milliseconds(std::max(1, 1000 / std::max(1, previewFps))));
            }, [cancelPreview]() {
                return cancelPreview->load();
            });
            QMetaObject::invokeMethod(this, [this, dialogGuard, labelGuard, cancelPreview]() {
                if (!cancelPreview->load() && !dialogGuard.isNull() && !labelGuard.isNull()) {
                    labelGuard->setText(QStringLiteral("实时预览播放完成"));
                }
                appendLog(QStringLiteral("实时预览耗时：") + formatDuration(renderTimer_.elapsed()));
            }, Qt::QueuedConnection);
        } catch (const std::exception& error) {
            QMetaObject::invokeMethod(this, [this, message = QString::fromUtf8(error.what())]() {
                QMessageBox::critical(this, QStringLiteral("预览失败"), message);
                appendLog(QStringLiteral("预览错误：") + message);
            }, Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(this, [this]() {
            previewButton_->setEnabled(true);
            renderButton_->setEnabled(true);
        }, Qt::QueuedConnection);
    });
}

void DashboardWindow::appendLog(const QString& message) {
    if (logView_ != nullptr) {
        logView_->append(QStringLiteral("🌸 ") + message);
    }
}

QString DashboardWindow::describeAudioEnergy(const lireal::audio::AudioFrameEnergy& energy, int current, int total) {
    auto bar = [](float value) {
        const int width = 14;
        const int filled = std::clamp(static_cast<int>(value * width), 0, width);
        QString text;
        for (int index = 0; index < width; ++index) {
            text += index < filled ? QStringLiteral("♥") : QStringLiteral("·");
        }
        return text;
    };

    QString stems;
    for (const auto& stem : energy.stems) {
        stems += QStringLiteral("\n  %1\n    E %2  P %3  pan %4  depth %5")
            .arg(QString::fromUtf8(stem.name.c_str()))
            .arg(bar(stem.energy))
            .arg(bar(stem.presence))
            .arg(stem.pan, 0, 'f', 2)
            .arg(stem.depth, 0, 'f', 2);
    }

    return QStringLiteral(
        "🎧 音频处理状态\n"
        "帧 %1 / %2   时间 %3s\n\n"
        "总能量  %4\n"
        "低频    %5\n"
        "中频    %6\n"
        "高频    %7\n"
        "人声    %8\n"
        "鼓点    %9\n"
        "氛围    %10\n\n"
        "Beat    %11\n"
        "Drop    %12\n"
        "声场宽度 %13\n\n"
        "🌸 分轨 / 3D声场%14")
        .arg(current)
        .arg(total)
        .arg(energy.timeSeconds, 0, 'f', 2)
        .arg(bar(energy.rms))
        .arg(bar(energy.bass))
        .arg(bar(energy.mid))
        .arg(bar(energy.treble))
        .arg(bar(energy.vocal))
        .arg(bar(energy.percussion))
        .arg(bar(energy.ambience))
        .arg(bar(energy.beatPulse))
        .arg(bar(energy.dropIntensity))
        .arg(bar(energy.stereoWidth))
        .arg(stems);
}

lireal::render::RenderConfig DashboardWindow::collectConfig() const {
    lireal::render::RenderConfig config;
    config.backgroundImagePath = backgroundEdit_->text().toStdString();
    config.musicPath = musicEdit_->text().toStdString();
    config.lyricPath = lyricsEdit_->text().toStdString();
    config.outputVideoPath = outputEdit_->text().toStdString();
    config.songTitle = songTitleEdit_->text().trimmed().toStdString();
    config.artistName = artistEdit_->text().trimmed().toStdString();
    config.watermarkText = watermarkEdit_->text().trimmed().toStdString();
    const QSize resolution = resolutionCombo_->currentData().toSize();
    config.width = resolution.width();
    config.height = resolution.height();
    config.fps = fpsSpin_->value();
    config.parallaxStrength = parallaxSpin_->value();
    config.pulseStrength = pulseSpin_->value();
    config.spectrumRadius = spectrumRadiusSpin_->value();
    config.spectrumBarHeight = spectrumHeightSpin_->value();
    config.glowStrength = glowSpin_->value();
    config.encoderBackend = encoderBackendCombo_->currentData().toString().toStdString();
    config.encoderDevice = encoderDeviceCombo_->currentData().toString().toStdString();
    config.encoderPreset = encoderPresetCombo_->currentData().toString().toStdString();
    config.encoderCrf = encoderCrfSpin_->value();
    config.renderBackend = "webgpu";
    config.renderThreads = renderThreadsSpin_->value();
    config.enableBloom = bloomCheck_->isChecked();
    config.enableCircularCover = circularCoverCheck_->isChecked();
    config.enableMangaFilter = mangaFilterCheck_->isChecked();
    config.enableImpactFlash = impactFlashCheck_->isChecked();
    config.enableParticles = particlesCheck_->isChecked();
    config.lyricLayoutMode = lyricLayoutCombo_->currentIndex();
    return config;
}

bool DashboardWindow::validateConfig(lireal::render::RenderConfig& config) {
    const QString backgroundPath = QString::fromStdString(config.backgroundImagePath.string()).trimmed();
    const QString musicPath = QString::fromStdString(config.musicPath.string()).trimmed();
    const QString lyricPath = QString::fromStdString(config.lyricPath.string()).trimmed();
    QString outputPath = QString::fromStdString(config.outputVideoPath.string()).trimmed();

    if (backgroundPath.isEmpty() || musicPath.isEmpty() || lyricPath.isEmpty() || outputPath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("素材不完整"), QStringLiteral("请先选择背景、音乐、歌词和输出路径。"));
        return false;
    }

    const QStringList imageSuffixes = {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("webp"), QStringLiteral("bmp")};
    const QStringList audioSuffixes = {QStringLiteral("mp3"), QStringLiteral("wav"), QStringLiteral("flac"), QStringLiteral("aac"), QStringLiteral("ogg"), QStringLiteral("m4a")};
    const QStringList lyricSuffixes = {QStringLiteral("lrc"), QStringLiteral("txt")};

    auto requireReadableFile = [&](const QString& path, const QString& name, const QStringList& suffixes) -> bool {
        const QFileInfo info(path);
        if (!info.exists() || !info.isFile()) {
            QMessageBox::warning(this, QStringLiteral("文件不存在"), QStringLiteral("%1不存在：\n%2").arg(name, path));
            return false;
        }
        if (!info.isReadable()) {
            QMessageBox::warning(this, QStringLiteral("文件不可读"), QStringLiteral("%1不可读：\n%2").arg(name, path));
            return false;
        }
        if (!hasAllowedSuffix(path, suffixes)) {
            QMessageBox::warning(this, QStringLiteral("文件格式不支持"), QStringLiteral("%1格式不支持。\n当前文件：%2\n支持格式：%3").arg(name, path, describeSuffixes(suffixes)));
            return false;
        }
        if (info.size() <= 0) {
            QMessageBox::warning(this, QStringLiteral("文件为空"), QStringLiteral("%1文件为空：\n%2").arg(name, path));
            return false;
        }
        return true;
    };

    if (!requireReadableFile(backgroundPath, QStringLiteral("背景图片"), imageSuffixes)) {
        return false;
    }
    if (!requireReadableFile(musicPath, QStringLiteral("音乐文件"), audioSuffixes)) {
        return false;
    }
    if (!requireReadableFile(lyricPath, QStringLiteral("歌词文件"), lyricSuffixes)) {
        return false;
    }

    QFile lyricFile(lyricPath);
    if (lyricFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QByteArray sample = lyricFile.read(4096);
        if (!sample.contains('[') || !sample.contains(']')) {
            const auto result = QMessageBox::question(
                this,
                QStringLiteral("歌词可能不是 LRC"),
                QStringLiteral("歌词文件看起来没有时间轴标签，例如 [00:12.34]。\n仍然继续渲染吗？"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
            if (result != QMessageBox::Yes) {
                return false;
            }
        }
    }

    if (!outputPath.endsWith(QStringLiteral(".mp4"), Qt::CaseInsensitive)) {
        outputPath += QStringLiteral(".mp4");
        outputEdit_->setText(outputPath);
        appendLog(QStringLiteral("已自动补全输出扩展名：") + outputPath);
    }

    const QFileInfo outputInfo(outputPath);
    QDir outputDir = outputInfo.dir();
    if (!outputDir.exists()) {
        const auto result = QMessageBox::question(
            this,
            QStringLiteral("输出目录不存在"),
            QStringLiteral("输出目录不存在，是否自动创建？\n%1").arg(outputDir.absolutePath()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);
        if (result != QMessageBox::Yes || !outputDir.mkpath(QStringLiteral("."))) {
            QMessageBox::warning(this, QStringLiteral("无法创建目录"), QStringLiteral("无法创建输出目录：\n%1").arg(outputDir.absolutePath()));
            return false;
        }
    }

    if (outputInfo.exists()) {
        const auto result = QMessageBox::question(
            this,
            QStringLiteral("覆盖确认"),
            QStringLiteral("输出文件已经存在，是否覆盖？\n%1").arg(outputPath),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (result != QMessageBox::Yes) {
            return false;
        }
    }

    config.backgroundImagePath = backgroundPath.toStdString();
    config.musicPath = musicPath.toStdString();
    config.lyricPath = lyricPath.toStdString();
    config.outputVideoPath = outputPath.toStdString();
    appendLog(QStringLiteral("素材检查通过，准备开始渲染。"));
    return true;
}

} // namespace lireal::ui
