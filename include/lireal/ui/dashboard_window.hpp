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

#pragma once

#include "lireal/render/render_config.hpp"

#include <QFutureWatcher>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QProgressBar>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QSpinBox>
#include <QTextEdit>

#include <atomic>

class QDragEnterEvent;
class QDropEvent;

namespace lireal::render {
struct RenderProgress;
}

namespace lireal::audio {
struct AudioFrameEnergy;
}

namespace lireal::ui {

class DashboardWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit DashboardWindow(QWidget* parent = nullptr);
    ~DashboardWindow() override;

private slots:
    void chooseBackground();
    void chooseMusic();
    void chooseLyrics();
    void chooseOutput();
    void saveStylePreset();
    void loadStylePreset();
    void generatePreview();
    void startRender();
    void cancelRender();
    void openOutputFolder();
    void applyQualityPreset();

private:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void buildUi();
    void populateEncoderDeviceList();
    void appendLog(const QString& message);
    void updateRenderStats(const lireal::render::RenderProgress& progress);
    static QString describeAudioEnergy(const lireal::audio::AudioFrameEnergy& energy, int current, int total);
    lireal::render::RenderConfig collectConfig() const;
    bool validateConfig(lireal::render::RenderConfig& config);

    QLineEdit* backgroundEdit_ = nullptr;
    QLineEdit* musicEdit_ = nullptr;
    QLineEdit* lyricsEdit_ = nullptr;
    QLineEdit* outputEdit_ = nullptr;
    QLineEdit* songTitleEdit_ = nullptr;
    QLineEdit* artistEdit_ = nullptr;
    QLineEdit* watermarkEdit_ = nullptr;
    QComboBox* resolutionCombo_ = nullptr;
    QComboBox* qualityPresetCombo_ = nullptr;
    QComboBox* previewTimeCombo_ = nullptr;
    QComboBox* lyricLayoutCombo_ = nullptr;
    QComboBox* fpsModeCombo_ = nullptr;
    QDoubleSpinBox* previewTimeSpin_ = nullptr;
    QSpinBox* fpsSpin_ = nullptr;
    QDoubleSpinBox* parallaxSpin_ = nullptr;
    QDoubleSpinBox* pulseSpin_ = nullptr;
    QDoubleSpinBox* spectrumRadiusSpin_ = nullptr;
    QDoubleSpinBox* spectrumHeightSpin_ = nullptr;
    QDoubleSpinBox* glowSpin_ = nullptr;
    QComboBox* encoderBackendCombo_ = nullptr;
    QComboBox* encoderDeviceCombo_ = nullptr;
    QComboBox* encoderPresetCombo_ = nullptr;
    QSpinBox* encoderCrfSpin_ = nullptr;
    QSpinBox* renderThreadsSpin_ = nullptr;
    QCheckBox* bloomCheck_ = nullptr;
    QCheckBox* circularCoverCheck_ = nullptr;
    QCheckBox* mangaFilterCheck_ = nullptr;
    QCheckBox* impactFlashCheck_ = nullptr;
    QCheckBox* particlesCheck_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QLabel* renderStatsLabel_ = nullptr;
    QTextEdit* logView_ = nullptr;
    QPushButton* previewButton_ = nullptr;
    QPushButton* renderButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
    QPushButton* openFolderButton_ = nullptr;
    QPushButton* savePresetButton_ = nullptr;
    QPushButton* loadPresetButton_ = nullptr;
    QFutureWatcher<void>* renderWatcher_ = nullptr;
    std::atomic_bool cancelRequested_ = false;
    QElapsedTimer renderTimer_;
    qint64 lastProgressUiUpdateMs_ = -1;
    int lastLoggedProgressPercent_ = -1;
    bool renderHadError_ = false;
    QString lastOutputPath_;
};

} // namespace lireal::ui
