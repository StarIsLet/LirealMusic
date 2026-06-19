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

#include "lireal/render/video_renderer.hpp"

#include "lireal/audio/audio_analyzer.hpp"
#include "lireal/lyrics/lrc_parser.hpp"
#include "lireal/render/webgpu_backend.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QString>

#include <algorithm>
#include <array>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <list>
#include <mutex>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unordered_map>

#if LIREAL_HAS_OPENMP
#include <omp.h>
#endif

namespace lireal::render {
namespace {

cv::Mat coverResize(const cv::Mat& source, int width, int height) {
    const double scale = std::max(width / static_cast<double>(source.cols), height / static_cast<double>(source.rows));
    cv::Mat resized;
    cv::resize(source, resized, {}, scale, scale, cv::INTER_CUBIC);
    const int x = std::max(0, (resized.cols - width) / 2);
    const int y = std::max(0, (resized.rows - height) / 2);
    return resized(cv::Rect(x, y, width, height)).clone();
}

cv::Mat makeCircularImage(const cv::Mat& source, int diameter) {
    cv::Mat square = coverResize(source, diameter, diameter);
    cv::Mat result(square.size(), square.type(), cv::Scalar(0, 0, 0));
    cv::Mat mask(square.rows, square.cols, CV_8UC1, cv::Scalar(0));
    cv::circle(mask, {diameter / 2, diameter / 2}, diameter / 2 - 4, cv::Scalar(255), cv::FILLED, cv::LINE_AA);
    square.copyTo(result, mask);
    return result;
}

RenderConfig makeFastPreviewConfig(RenderConfig config) {
    if (!config.enableFastPreview) {
        return config;
    }

    const int maxWidth = std::clamp(config.previewMaxWidth, 640, 1920);
    const int maxHeight = std::clamp(config.previewMaxHeight, 360, 1080);
    const double scale = std::min({1.0, maxWidth / static_cast<double>(std::max(1, config.width)), maxHeight / static_cast<double>(std::max(1, config.height))});
    config.width = std::max(320, static_cast<int>(std::round(config.width * scale / 2.0)) * 2);
    config.height = std::max(180, static_cast<int>(std::round(config.height * scale / 2.0)) * 2);
    config.fps = std::clamp(config.previewFps, 12, std::min(60, std::max(12, config.fps)));
    config.renderBatchFrames = std::max(config.renderBatchFrames, config.fps / 2);
    config.spectrumBarHeight *= 0.62;
    config.glowStrength *= 0.52;
    config.parallaxStrength *= 0.72;
    config.enableBloom = false;
    config.enableMangaFilter = false;
    config.enableImpactFlash = false;
    config.enableParticles = false;
    return config;
}

struct CachedTextPath {
    QPainterPath path;
    qreal width = 0.0;
    qreal ascent = 0.0;
    qreal descent = 0.0;
    qreal height = 0.0;
};

struct TextCacheKey {
    QString text;
    QString family;
    int pixelSize = 0;
    int weight = 0;
    bool operator==(const TextCacheKey& other) const {
        return pixelSize == other.pixelSize && weight == other.weight && family == other.family && text == other.text;
    }
};

struct TextCacheKeyHash {
    std::size_t operator()(const TextCacheKey& key) const noexcept {
        return qHash(key.text) ^ (qHash(key.family) << 1U) ^ (static_cast<std::size_t>(key.pixelSize) << 16U) ^ static_cast<std::size_t>(key.weight);
    }
};

class TextPathCache {
public:
    CachedTextPath get(const QString& text, const QFont& font) {
        TextCacheKey key{text, font.family(), font.pixelSize(), font.weight()};
        std::scoped_lock lock(mutex_);
        auto found = cache_.find(key);
        if (found != cache_.end()) {
            return found->second;
        }
        if (cache_.size() >= maxEntries_) {
            cache_.erase(order_.front());
            order_.pop_front();
        }
        CachedTextPath value;
        value.path.addText(QPointF(0.0, 0.0), font, text);
        const QFontMetricsF metrics(font);
        value.width = metrics.horizontalAdvance(text);
        value.ascent = metrics.ascent();
        value.descent = metrics.descent();
        value.height = metrics.height();
        auto [inserted, _] = cache_.emplace(key, value);
        order_.push_back(std::move(key));
        return inserted->second;
    }

private:
    static constexpr std::size_t maxEntries_ = 768;
    std::mutex mutex_;
    std::unordered_map<TextCacheKey, CachedTextPath, TextCacheKeyHash> cache_;
    std::list<TextCacheKey> order_;
};

TextPathCache& textPathCache() {
    static TextPathCache cache;
    return cache;
}

CachedTextPath cachedTextPath(const QString& text, const QFont& font) {
    struct HotEntry {
        QString text;
        QString family;
        int pixelSize = 0;
        int weight = 0;
        CachedTextPath value;
        bool valid = false;
    };
    thread_local std::array<HotEntry, 16> hotEntries;
    const int pixelSize = font.pixelSize();
    const int weight = font.weight();
    const QString family = font.family();
    for (const HotEntry& entry : hotEntries) {
        if (entry.valid && entry.pixelSize == pixelSize && entry.weight == weight && entry.family == family && entry.text == text) {
            return entry.value;
        }
    }
    static thread_local std::size_t cursor = 0;
    CachedTextPath value = textPathCache().get(text, font);
    HotEntry& slot = hotEntries[cursor++ % hotEntries.size()];
    slot.text = text;
    slot.family = family;
    slot.pixelSize = pixelSize;
    slot.weight = weight;
    slot.value = value;
    slot.valid = true;
    return value;
}

void drawCachedTextPath(QPainter& painter, const CachedTextPath& cached, QPointF position, const QColor& fill, const QColor& outline, qreal outlineWidth) {
    QPainterPath translated = cached.path;
    translated.translate(position);
    painter.setPen(QPen(outline, outlineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(fill);
    painter.drawPath(translated);
}

bool shouldUseWebGpuShaders(const RenderConfig& config) {
#if LIREAL_HAS_WEBGPU
    return config.renderBackend.find("webgpu") != std::string::npos;
#else
    (void)config;
    return false;
#endif
}

void alphaBlend(cv::Mat& dst, const cv::Mat& src, cv::Point topLeft, double alpha) {
    const cv::Rect dstRect(0, 0, dst.cols, dst.rows);
    const cv::Rect srcRect(topLeft.x, topLeft.y, src.cols, src.rows);
    const cv::Rect clipped = dstRect & srcRect;
    if (clipped.empty()) {
        return;
    }

    const cv::Rect sourceRoi(clipped.x - topLeft.x, clipped.y - topLeft.y, clipped.width, clipped.height);
    cv::Mat dstPart = dst(clipped);
    cv::Mat srcPart = src(sourceRoi);
    cv::addWeighted(srcPart, alpha, dstPart, 1.0 - alpha, 0.0, dstPart);
}

void alphaBlendCircle(cv::Mat& dst, const cv::Mat& src, cv::Point topLeft, double alpha) {
    const cv::Rect dstRect(0, 0, dst.cols, dst.rows);
    const cv::Rect srcRect(topLeft.x, topLeft.y, src.cols, src.rows);
    const cv::Rect clipped = dstRect & srcRect;
    if (clipped.empty()) {
        return;
    }

    const cv::Rect sourceRoi(clipped.x - topLeft.x, clipped.y - topLeft.y, clipped.width, clipped.height);
    cv::Mat mask(src.rows, src.cols, CV_8UC1, cv::Scalar(0));
    const int diameter = std::min(src.cols, src.rows);
    cv::circle(mask, {src.cols / 2, src.rows / 2}, diameter / 2 - 3, cv::Scalar(255), cv::FILLED, cv::LINE_AA);
    cv::GaussianBlur(mask, mask, {0, 0}, 1.4);

    cv::Mat dstPart = dst(clipped);
    cv::Mat srcPart = src(sourceRoi);
    cv::Mat blended;
    cv::addWeighted(srcPart, alpha, dstPart, 1.0 - alpha, 0.0, blended);
    blended.copyTo(dstPart, mask(sourceRoi));
}

void drawSpectrumRing(cv::Mat& frame, const std::vector<float>& bins, cv::Point center, double radius, double barHeight, const cv::Scalar& color) {
    if (bins.empty()) {
        return;
    }

    cv::Mat glow = cv::Mat::zeros(frame.size(), frame.type());
    cv::Mat crisp = cv::Mat::zeros(frame.size(), frame.type());

    static const std::array<double, 64> cosTable = []() {
        std::array<double, 64> values{};
        for (std::size_t index = 0; index < values.size(); ++index) {
            const double angle = (2.0 * CV_PI * static_cast<double>(index) / static_cast<double>(values.size())) - CV_PI / 2.0;
            values[index] = std::cos(angle);
        }
        return values;
    }();
    static const std::array<double, 64> sinTable = []() {
        std::array<double, 64> values{};
        for (std::size_t index = 0; index < values.size(); ++index) {
            const double angle = (2.0 * CV_PI * static_cast<double>(index) / static_cast<double>(values.size())) - CV_PI / 2.0;
            values[index] = std::sin(angle);
        }
        return values;
    }();

    for (std::size_t index = 0; index < bins.size(); ++index) {
        const std::size_t tableIndex = index % cosTable.size();
        const double level = std::clamp(static_cast<double>(bins[index]), 0.0, 1.0);
        const double harmonic = 0.5 + 0.5 * std::sin(static_cast<double>(index) * 0.37 + level * 2.6);
        const cv::Scalar reactiveColor(
            std::clamp(color[0] + 16.0 * harmonic, 0.0, 255.0),
            std::clamp(color[1] - 18.0 + level * 54.0, 0.0, 255.0),
            std::clamp(color[2] - 8.0 + harmonic * 42.0, 0.0, 255.0));
        const double inner = radius - 3.0 - level * 5.0;
        const double outer = radius + 12.0 + barHeight * (0.22 + level * 1.08);
        const cv::Point p1(
            center.x + static_cast<int>(cosTable[tableIndex] * inner),
            center.y + static_cast<int>(sinTable[tableIndex] * inner));
        const cv::Point p2(
            center.x + static_cast<int>(cosTable[tableIndex] * outer),
            center.y + static_cast<int>(sinTable[tableIndex] * outer));
        const int thickness = 2 + static_cast<int>(level * 4.0);
        cv::line(glow, p1, p2, reactiveColor, thickness + 7, cv::LINE_AA);
        cv::line(crisp, p1, p2, reactiveColor, thickness, cv::LINE_AA);

        if (index % 2U == 0U) {
            const double mirrorOuter = radius - 8.0 - barHeight * 0.26 * level;
            const cv::Point p3(
                center.x + static_cast<int>(cosTable[tableIndex] * mirrorOuter),
                center.y + static_cast<int>(sinTable[tableIndex] * mirrorOuter));
            cv::line(glow, p1, p3, reactiveColor, std::max(1, thickness - 1), cv::LINE_AA);
        }
    }

    cv::GaussianBlur(glow, glow, {0, 0}, 4.8);
    cv::addWeighted(glow, 0.62, frame, 1.0, 0.0, frame);
    cv::addWeighted(crisp, 0.95, frame, 1.0, 0.0, frame);
}

QFont makeLyricFont(int pixelSize, bool active, const std::string& requestedFamily = {});

void loadBundledCuteFonts() {
    static const bool loaded = []() {
        const std::filesystem::path current = std::filesystem::current_path();
        const std::initializer_list<std::filesystem::path> candidates = {
            current / "loli.ttf",
            current / "萝莉体 第二版" / "loli.ttf",
            current / "assets" / "fonts" / "loli.ttf"
        };
        for (const auto& path : candidates) {
            if (std::filesystem::exists(path)) {
                QFontDatabase::addApplicationFont(QString::fromStdString(path.string()));
            }
        }
        return true;
    }();
    (void)loaded;
}

cv::Scalar estimateLightAccentColor(const cv::Mat& background) {
    cv::Scalar meanColor = cv::mean(background);
    const double blue = std::clamp(meanColor[0] * 0.72 + 70.0, 150.0, 245.0);
    const double green = std::clamp(meanColor[1] * 0.70 + 78.0, 160.0, 245.0);
    const double red = std::clamp(meanColor[2] * 0.66 + 86.0, 170.0, 255.0);
    return {blue, green, red};
}

void drawSoftSnow(cv::Mat& frame, double timeSeconds, const cv::Scalar& accent) {
    cv::Mat layer = cv::Mat::zeros(frame.size(), frame.type());
    constexpr int snowCount = 92;
    for (int index = 0; index < snowCount; ++index) {
        const double seed = static_cast<double>(index + 11) * 19.917;
        const double rx = std::sin(seed) * 43758.5453;
        const double ry = std::cos(seed * 1.37) * 24634.6345;
        const double baseX = rx - std::floor(rx);
        const double baseY = ry - std::floor(ry);
        const double drift = std::fmod(baseY + timeSeconds * (0.018 + (index % 7) * 0.003), 1.0);
        const int x = static_cast<int>(baseX * frame.cols + std::sin(timeSeconds * 0.42 + seed) * 24.0);
        const int y = static_cast<int>(drift * frame.rows);
        const int radius = 1 + (index % 3);
        cv::circle(layer, {std::clamp(x, 0, frame.cols - 1), std::clamp(y, 0, frame.rows - 1)}, radius, accent, cv::FILLED, cv::LINE_AA);
    }
    cv::GaussianBlur(layer, layer, {0, 0}, 0.8);
    cv::addWeighted(layer, 0.44, frame, 1.0, 0.0, frame);
}

void drawGlowingText(QPainter& painter, QString text, QPointF position, QFont font, const QColor& fill, const QColor& glow, qreal glowWidth) {
    const CachedTextPath cached = cachedTextPath(text, font);
    drawCachedTextPath(painter, cached, position, fill, glow, glowWidth);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    QPainterPath translated = cached.path;
    translated.translate(position);
    painter.drawPath(translated);
}

void drawHighLightedCuteText(QPainter& painter, QString text, QPointF position, QFont font, const QColor& fill, const QColor& glow, double timeSeconds, qreal glowWidth) {
    const CachedTextPath cached = cachedTextPath(text, font);
    const qreal textWidth = cached.width;
    const QRectF bounds(position.x() - textWidth * 0.05, position.y() - cached.ascent * 1.05, textWidth * 1.10, cached.height * 1.45);
    const qreal pulse = 0.5 + 0.5 * std::sin(timeSeconds * 1.8);
    const qreal sweep = std::fmod(timeSeconds * 0.34, 1.0);

    painter.save();
    painter.setOpacity(0.34 + pulse * 0.16);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 72));
    painter.drawRoundedRect(bounds, bounds.height() * 0.45, bounds.height() * 0.45);
    painter.restore();

    QPainterPath path = cached.path;
    path.translate(position);
    painter.save();
    painter.setPen(QPen(glow, glowWidth + pulse * 5.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(fill);
    painter.drawPath(path);
    painter.restore();

    painter.save();
    QLinearGradient gradient(position.x(), 0.0, position.x() + textWidth, 0.0);
    gradient.setColorAt(0.0, fill);
    gradient.setColorAt(std::clamp(sweep - 0.12, 0.0, 1.0), QColor(255, 255, 255));
    gradient.setColorAt(std::clamp(sweep, 0.0, 1.0), QColor(180, 220, 255));
    gradient.setColorAt(std::clamp(sweep + 0.12, 0.0, 1.0), fill);
    gradient.setColorAt(1.0, fill);
    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawPath(path);
    painter.restore();

    painter.save();
    painter.setOpacity(0.55 + pulse * 0.25);
    painter.setPen(QPen(QColor(255, 255, 255, 210), std::max<qreal>(1.5, glowWidth * 0.20), Qt::SolidLine, Qt::RoundCap));
    for (int star = 0; star < 4; ++star) {
        const qreal sx = bounds.left() + bounds.width() * std::fmod(0.17 + star * 0.23 + timeSeconds * 0.035, 1.0);
        const qreal sy = bounds.top() + bounds.height() * (0.20 + 0.18 * std::sin(timeSeconds * 1.2 + star));
        const qreal r = 4.0 + pulse * 4.0 + star;
        painter.drawLine(QPointF(sx - r, sy), QPointF(sx + r, sy));
        painter.drawLine(QPointF(sx, sy - r), QPointF(sx, sy + r));
    }
    painter.restore();
}

void drawSongInfo(cv::Mat& frame, const RenderConfig& config, const cv::Scalar& accentColor, double timeSeconds) {
    QImage image(frame.data, frame.cols, frame.rows, static_cast<int>(frame.step), QImage::Format_BGR888);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QString title = QStringLiteral("《%1》").arg(QString::fromUtf8(config.songTitle.c_str()).trimmed().isEmpty() ? QStringLiteral("未命名") : QString::fromUtf8(config.songTitle.c_str()).trimmed());
    const QString artist = QString::fromUtf8(config.artistName.c_str()).trimmed().isEmpty() ? QStringLiteral("未知作者") : QString::fromUtf8(config.artistName.c_str()).trimmed();
    const QColor glow(static_cast<int>(accentColor[2]), static_cast<int>(accentColor[1]), static_cast<int>(accentColor[0]), 190);

    QFont titleFont = makeLyricFont(std::max(32, frame.cols / 26), true, config.lyricFontFamily);
    QFont artistFont = makeLyricFont(std::max(20, frame.cols / 66), false, config.lyricFontFamily);
    const QFontMetricsF titleMetrics(titleFont);
    const QFontMetricsF artistMetrics(artistFont);
    const qreal minDim = std::min(frame.cols, frame.rows);
    const qreal coverCenterX = frame.cols * 0.27;
    const qreal coverCenterY = frame.rows * 0.57;
    const qreal blackRingRadius = minDim * 0.176;
    const qreal spectrumRadius = blackRingRadius + minDim * 0.022;
    const qreal spectrumBarHeight = config.spectrumBarHeight * 0.58;
    const qreal outerRingTop = coverCenterY - spectrumRadius - spectrumBarHeight - minDim * 0.030;
    const qreal titleBlockGap = minDim * 0.024;
    const qreal artistBaselineY = std::max(titleMetrics.height() + artistMetrics.height() + minDim * 0.020, outerRingTop - titleBlockGap);
    const qreal titleY = artistBaselineY - artistMetrics.height() * 0.98;
    const qreal titleX = coverCenterX - titleMetrics.horizontalAdvance(title) * 0.5;
    painter.setOpacity(0.98);
    drawHighLightedCuteText(painter, title, QPointF(titleX, titleY), titleFont, QColor(255, 255, 255), glow, timeSeconds, 6.8);
    drawHighLightedCuteText(painter, artist, QPointF(coverCenterX - artistMetrics.horizontalAdvance(artist) * 0.5, artistBaselineY), artistFont, QColor(255, 255, 255, 240), QColor(150, 205, 255, 150), timeSeconds + 0.7, 3.4);
    painter.end();
}

void drawWatermark(cv::Mat& frame, const RenderConfig& config) {
    QImage image(frame.data, frame.cols, frame.rows, static_cast<int>(frame.step), QImage::Format_BGR888);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    const QString text = QString::fromUtf8(config.watermarkText.c_str()).trimmed().isEmpty() ? QStringLiteral("Lireal Music") : QString::fromUtf8(config.watermarkText.c_str()).trimmed();
    QFont font = makeLyricFont(std::max(16, frame.cols / 78), false, config.lyricFontFamily);
    const QFontMetricsF metrics(font);
    painter.setFont(font);
    painter.setPen(QColor(255, 255, 255, 170));
    painter.drawText(QPointF(frame.cols - metrics.horizontalAdvance(text) - frame.cols * 0.045, frame.rows * 0.06), text);
    painter.end();
}

const lyrics::LyricLine* activeLyric(const std::vector<lyrics::LyricLine>& lines, double timeSeconds) {
    for (const auto& line : lines) {
        if (timeSeconds >= line.startSeconds && timeSeconds < line.endSeconds) {
            return &line;
        }
    }
    return nullptr;
}

QFont makeLyricFont(int pixelSize, bool active, const std::string& requestedFamily) {
    loadBundledCuteFonts();
    static const QString fallbackFamily = []() {
        const QStringList preferredFamilies = {
            QStringLiteral("萝莉体"),
            QStringLiteral("汉仪萝莉体简"),
            QStringLiteral("HYLiLiangHeiJ"),
            QStringLiteral("Aa萝莉体"),
            QStringLiteral("AaLoli"),
            QStringLiteral("Lolita"),
            QStringLiteral("loli"),
            QStringLiteral("LXGW WenKai"),
            QStringLiteral("LXGW WenKai Screen"),
            QStringLiteral("ZCOOL KuaiLe"),
            QStringLiteral("ZCOOL QingKe HuangYou"),
            QStringLiteral("Ma Shan Zheng"),
            QStringLiteral("YouYuan"),
            QStringLiteral("Noto Sans CJK SC"),
            QStringLiteral("Noto Sans CJK JP"),
            QStringLiteral("Source Han Sans SC"),
            QStringLiteral("WenQuanYi Micro Hei"),
            QStringLiteral("Microsoft YaHei"),
            QStringLiteral("Sans Serif")
        };
        const QStringList installedFamilies = QFontDatabase::families();
        for (const QString& family : preferredFamilies) {
            if (installedFamilies.contains(family)) {
                return family;
            }
        }
        return QStringLiteral("Sans Serif");
    }();

    const QString requested = QString::fromUtf8(requestedFamily.c_str()).trimmed();
    const QStringList installedFamilies = QFontDatabase::families();
    const QString chosenFamily = (!requested.isEmpty() && installedFamilies.contains(requested)) ? requested : fallbackFamily;
    QFont font(chosenFamily);
    font.setPixelSize(pixelSize);
    font.setWeight(active ? QFont::ExtraBold : QFont::DemiBold);
    font.setStyleStrategy(static_cast<QFont::StyleStrategy>(QFont::PreferAntialias | QFont::PreferQuality));
    font.setHintingPreference(QFont::PreferVerticalHinting);
    return font;
}

void drawOutlinedText(QPainter& painter, const QString& text, QPointF position, const QFont& font, const QColor& fill, const QColor& outline, qreal outlineWidth) {
    const CachedTextPath cached = cachedTextPath(text, font);
    drawCachedTextPath(painter, cached, position, fill, outline, outlineWidth);
}

QFont fitLyricFont(QString& text, int preferredPixelSize, int minPixelSize, qreal maxWidth, bool active) {
    QFont font = makeLyricFont(preferredPixelSize, active);
    for (int pixelSize = preferredPixelSize; pixelSize >= minPixelSize; pixelSize -= 2) {
        font.setPixelSize(pixelSize);
        const QFontMetricsF metrics(font);
        if (metrics.horizontalAdvance(text) <= maxWidth) {
            return font;
        }
    }

    font.setPixelSize(minPixelSize);
    const QFontMetricsF metrics(font);
    text = metrics.elidedText(text, Qt::ElideRight, maxWidth);
    return font;
}

void drawCenteredOutlinedText(QPainter& painter, QString text, const QRectF& area, int preferredPixelSize, int minPixelSize, bool active, const QColor& fill, const QColor& outline, qreal outlineWidth) {
    QFont font = fitLyricFont(text, preferredPixelSize, minPixelSize, area.width(), active);
    const QFontMetricsF metrics(font);
    const QPointF position(area.center().x() - metrics.horizontalAdvance(text) / 2.0, area.center().y() + metrics.ascent() / 2.0 - metrics.descent());
    drawOutlinedText(painter, text, position, font, fill, outline, outlineWidth);
}

void drawAnimatedLyricText(
    QPainter& painter,
    QString text,
    const QRectF& area,
    int preferredPixelSize,
    int minPixelSize,
    bool active,
    bool previousActive,
    bool nextActive,
    const QColor& fill,
    const QColor& outline,
    qreal outlineWidth,
    double lineProgress,
    double energy) {
    QFont font = fitLyricFont(text, preferredPixelSize, minPixelSize, area.width(), active);
    const CachedTextPath cached = cachedTextPath(text, font);
    const qreal textWidth = cached.width;
    const qreal baseline = area.center().y() + cached.ascent / 2.0 - cached.descent;
    const qreal entrance = active ? std::clamp(lineProgress / 0.22, 0.0, 1.0) : (nextActive ? 0.0 : 1.0);
    const qreal exit = active ? std::clamp((1.0 - lineProgress) / 0.24, 0.0, 1.0) : (previousActive ? 0.0 : 1.0);
    const qreal life = active ? std::min(entrance, exit) : (previousActive || nextActive ? 0.64 : 1.0);
    const qreal pop = active ? std::sin(std::clamp(lineProgress / 0.28, 0.0, 1.0) * M_PI) : 0.0;
    const qreal switchSlide = previousActive ? -area.width() * 0.11 : (nextActive ? area.width() * 0.11 : 0.0);
    const qreal slide = active ? (1.0 - entrance) * area.width() * 0.16 - (1.0 - exit) * area.width() * 0.10 : switchSlide;
    const qreal floatY = active ? std::sin(lineProgress * M_PI * 2.0) * (3.0 + energy * 7.0) - pop * area.height() * 0.14 : (previousActive ? -area.height() * 0.18 : (nextActive ? area.height() * 0.14 : 0.0));
    painter.save();
    painter.translate(area.center());
    const qreal scale = active ? 0.94 + pop * 0.10 + energy * 0.018 : (previousActive || nextActive ? 0.94 : 1.0);
    painter.scale(scale, scale);
    const QPointF position(-textWidth / 2.0 + slide, baseline + floatY - area.center().y());

    if (active) {
        painter.save();
        painter.setOpacity(std::clamp(0.14 + energy * 0.18, 0.12, 0.34) * life);
        QPainterPath glowPath = cached.path;
        glowPath.translate(position);
        painter.setPen(QPen(QColor(172, 211, 255, 130), outlineWidth + 12.0 + energy * 10.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(glowPath);
        painter.restore();
    }

    drawCachedTextPath(painter, cached, position, fill, outline, outlineWidth);

    if (active) {
        const qreal fillWidth = std::clamp(lineProgress, 0.0, 1.0) * textWidth;
        painter.save();
        painter.setClipRect(QRectF(position.x() - 8.0, position.y() - cached.ascent - 8.0, fillWidth + 16.0, cached.height + 18.0));
        drawCachedTextPath(painter, cached, position, QColor(160, 210, 255), QColor(255, 255, 255, 180), outlineWidth * 0.52);
        painter.restore();

        painter.save();
        const qreal sweepX = position.x() + fillWidth;
        painter.setOpacity(std::clamp(0.22 + energy * 0.28, 0.18, 0.52) * life);
        painter.setPen(QPen(QColor(255, 255, 255, 210), std::max<qreal>(2.0, outlineWidth * 0.42), Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(sweepX, position.y() - cached.ascent * 0.78), QPointF(sweepX + 10.0, position.y() + cached.descent + 6.0));
        painter.restore();
    }
    painter.restore();
}

int activeLyricIndex(const std::vector<lyrics::LyricLine>& lines, double timeSeconds) {
    if (lines.empty()) {
        return -1;
    }

    int nearest = 0;
    for (int index = 0; index < static_cast<int>(lines.size()); ++index) {
        const auto& line = lines[static_cast<std::size_t>(index)];
        if (timeSeconds >= line.startSeconds && timeSeconds < line.endSeconds) {
            return index;
        }
        if (line.startSeconds <= timeSeconds) {
            nearest = index;
        }
    }
    return nearest;
}

void applyMangaFilter(cv::Mat& frame, float intensity) {
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::Mat edges;
    cv::Canny(gray, edges, 70, 150);
    cv::Mat edgesBgr;
    cv::cvtColor(edges, edgesBgr, cv::COLOR_GRAY2BGR);

    cv::Mat mono;
    cv::cvtColor(gray, mono, cv::COLOR_GRAY2BGR);
    cv::addWeighted(frame, 1.0 - intensity, mono, intensity, 0.0, frame);
    frame.setTo(cv::Scalar(12, 12, 16), edges);
}

void addImpactFlash(cv::Mat& frame, float bassEnergy) {
    if (bassEnergy < 0.56F) {
        return;
    }
    const double alpha = std::clamp((bassEnergy - 0.56F) * 1.15F, 0.0F, 0.38F);
    cv::Mat flash(frame.size(), frame.type(), cv::Scalar(255, 238, 250));
    cv::addWeighted(flash, alpha, frame, 1.0 - alpha, 0.0, frame);
}

void drawAuroraRibbons(cv::Mat& frame, const audio::AudioFrameEnergy& energy, double timeSeconds) {
    cv::Mat layer = cv::Mat::zeros(frame.size(), frame.type());
    const int lanes = 5;
    const int points = 96;
    for (int lane = 0; lane < lanes; ++lane) {
        std::vector<cv::Point> polyline;
        polyline.reserve(points);
        const double phase = timeSeconds * (0.42 + lane * 0.07) + lane * 1.31;
        const double baseY = frame.rows * (0.24 + lane * 0.105);
        const double amp = frame.rows * (0.020 + energy.vocal * 0.035 + energy.dropIntensity * 0.030 + lane * 0.004);
        for (int i = 0; i < points; ++i) {
            const double xRatio = static_cast<double>(i) / static_cast<double>(points - 1);
            const std::size_t binIndex = energy.spectrumBins.empty() ? 0U : static_cast<std::size_t>((i * energy.spectrumBins.size()) / points) % energy.spectrumBins.size();
            const double bin = energy.spectrumBins.empty() ? energy.rms : energy.spectrumBins[binIndex];
            const double wave = std::sin(xRatio * CV_PI * (2.2 + lane * 0.45) + phase) + std::sin(xRatio * CV_PI * 6.0 - phase * 1.7) * 0.35;
            const int x = static_cast<int>(xRatio * frame.cols);
            const int y = static_cast<int>(baseY + wave * amp + (bin - 0.5) * frame.rows * 0.055);
            polyline.emplace_back(std::clamp(x, 0, frame.cols - 1), std::clamp(y, 0, frame.rows - 1));
        }
        const cv::Scalar color(
            255,
            182 + lane * 10 + energy.colorMood * 34.0F,
            218 + energy.vocal * 36.0F + lane * 5);
        cv::polylines(layer, polyline, false, color, 2 + static_cast<int>(energy.dropIntensity * 4.0F), cv::LINE_AA);
    }
    cv::GaussianBlur(layer, layer, {0, 0}, 5.5 + energy.ambience * 4.0 + energy.dropIntensity * 2.5);
    cv::addWeighted(layer, 0.24 + energy.rms * 0.20 + energy.beatPulse * 0.18, frame, 1.0, 0.0, frame);
}

void drawBassHalo(cv::Mat& frame, const audio::AudioFrameEnergy& energy, cv::Point center, int radius) {
    if (energy.bass < 0.10F && energy.beatPulse < 0.10F) {
        return;
    }
    cv::Mat layer = cv::Mat::zeros(frame.size(), frame.type());
    const float power = std::clamp(energy.bass * 0.62F + energy.beatPulse * 0.52F + energy.dropIntensity * 0.34F, 0.0F, 1.0F);
    for (int ring = 0; ring < 4; ++ring) {
        const int r = radius + static_cast<int>((18 + ring * 24) * (0.6F + power));
        const int thickness = 2 + static_cast<int>(power * 7.0F) - ring;
        const cv::Scalar color(255, 205 + ring * 9, 238 + energy.colorMood * 15.0F);
        cv::circle(layer, center, r, color, std::max(1, thickness), cv::LINE_AA);
    }
    cv::GaussianBlur(layer, layer, {0, 0}, 7.0 + power * 6.0);
    cv::addWeighted(layer, 0.28 + power * 0.44, frame, 1.0, 0.0, frame);
}

void drawParticles(cv::Mat& frame, double timeSeconds, float energy) {
    cv::Mat layer = cv::Mat::zeros(frame.size(), frame.type());
    constexpr int particleCount = 168;
    for (int index = 0; index < particleCount; ++index) {
        const double seed = static_cast<double>(index + 1) * 12.9898;
        const double rawX = std::sin(seed) * 43758.5453;
        const double rawY = std::cos(seed * 1.71) * 24634.6345;
        const double baseX = rawX - std::floor(rawX);
        const double baseY = rawY - std::floor(rawY);
        const double drift = std::fmod(timeSeconds * (0.035 + index % 7 * 0.006) + baseY, 1.0);
        const int x = static_cast<int>(baseX * frame.cols + std::sin(timeSeconds * 1.4 + index) * 42.0 * energy);
        const int y = static_cast<int>(drift * frame.rows);
        const int size = 1 + (index % 4) + static_cast<int>(energy * 3.0F);
        const cv::Scalar color = index % 3 == 0 ? cv::Scalar(255, 225 + energy * 25.0F, 245) : cv::Scalar(235, 235, 248);
        cv::circle(layer, {std::clamp(x, 0, frame.cols - 1), std::clamp(y, 0, frame.rows - 1)}, size, color, cv::FILLED, cv::LINE_AA);
    }
    cv::GaussianBlur(layer, layer, {0, 0}, 0.9 + energy * 1.6);
    cv::addWeighted(layer, 0.62 + energy * 0.22, frame, 1.0, 0.0, frame);
}

void drawAudioComets(cv::Mat& frame, const audio::AudioFrameEnergy& energy, double timeSeconds) {
    cv::Mat layer = cv::Mat::zeros(frame.size(), frame.type());
    const int cometCount = 18;
    const cv::Point center(static_cast<int>(frame.cols * 0.50), static_cast<int>(frame.rows * 0.54));
    for (int index = 0; index < cometCount; ++index) {
        const double seed = static_cast<double>(index + 3) * 2.371;
        const double level = energy.spectrumBins.empty() ? energy.rms : energy.spectrumBins[static_cast<std::size_t>((index * 7) % energy.spectrumBins.size())];
        const double angle = timeSeconds * (0.55 + index * 0.012) + seed + energy.stereoWidth * 0.7;
        const double orbit = (0.18 + 0.035 * index + level * 0.14) * std::min(frame.cols, frame.rows);
        const cv::Point head(
            center.x + static_cast<int>(std::cos(angle) * orbit * (1.15 + energy.stereoWidth * 0.30)),
            center.y + static_cast<int>(std::sin(angle * 0.86) * orbit * (0.62 + energy.ambience * 0.18)));
        const cv::Point tail(
            head.x - static_cast<int>(std::cos(angle) * (32.0 + level * 110.0)),
            head.y - static_cast<int>(std::sin(angle * 0.86) * (20.0 + level * 80.0)));
        const cv::Scalar color(255, 190 + level * 55.0, 215 + energy.colorMood * 38.0);
        cv::line(layer, tail, head, color, 1 + static_cast<int>(level * 4.0), cv::LINE_AA);
        cv::circle(layer, head, 2 + static_cast<int>(level * 7.0), color, cv::FILLED, cv::LINE_AA);
    }
    cv::GaussianBlur(layer, layer, {0, 0}, 2.2 + energy.ambience * 2.8);
    cv::addWeighted(layer, 0.34 + energy.beatPulse * 0.22, frame, 1.0, 0.0, frame);
}

cv::Point projectStemPoint(const audio::AudioStemEnergy& stem, const cv::Size& size, double orbitPhase) {
    const double depth = std::clamp(static_cast<double>(stem.depth), 0.0, 1.0);
    const double perspective = 1.18 - depth * 0.50;
    const double width = 0.20 + static_cast<double>(stem.width) * 0.26;
    const double x = size.width * (0.50 + stem.pan * width * perspective + std::sin(orbitPhase) * stem.width * 0.055 * perspective);
    const double y = size.height * (0.63 - stem.height * 0.34 + depth * 0.22 + std::cos(orbitPhase) * stem.width * 0.040);
    return {static_cast<int>(std::clamp(x, 0.0, static_cast<double>(size.width - 1))), static_cast<int>(std::clamp(y, 0.0, static_cast<double>(size.height - 1)))};
}

cv::Scalar stemColor(const audio::AudioStemEnergy& stem) {
    if (stem.name.find("Vocal") != std::string::npos) {
        return cv::Scalar(255, 190, 255);
    }
    if (stem.name.find("Harmony") != std::string::npos) {
        return cv::Scalar(255, 220, 210);
    }
    if (stem.name.find("Bass") != std::string::npos) {
        return cv::Scalar(220, 145, 255);
    }
    if (stem.name.find("Percussion") != std::string::npos) {
        return cv::Scalar(255, 245, 190);
    }
    return cv::Scalar(255, 235, 245);
}

void drawSurroundStage(cv::Mat& frame, const audio::AudioFrameEnergy& energy, double timeSeconds) {
    if (energy.stems.empty()) {
        return;
    }

    cv::Mat layer = cv::Mat::zeros(frame.size(), frame.type());
    const cv::Point stageCenter(static_cast<int>(frame.cols * 0.50), static_cast<int>(frame.rows * 0.64));
    const int baseRadiusX = static_cast<int>(frame.cols * (0.27 + energy.stereoWidth * 0.17));
    const int baseRadiusY = static_cast<int>(frame.rows * (0.095 + energy.ambience * 0.035));
    for (int ring = 0; ring < 6; ++ring) {
        const double scale = 0.74 + ring * 0.24 + energy.ambience * 0.18;
        const double yOffset = (ring - 2.0) * frame.rows * 0.018;
        const cv::Scalar color(205 + ring * 6, 155 + ring * 15, 255);
        cv::ellipse(layer, {stageCenter.x, stageCenter.y + static_cast<int>(yOffset)}, {static_cast<int>(baseRadiusX * scale), static_cast<int>(baseRadiusY * scale)}, 0.0, 0.0, 360.0, color, 1 + ring / 2, cv::LINE_AA);
    }

    const int speakerY = static_cast<int>(frame.rows * 0.62);
    const int speakerOffset = static_cast<int>(frame.cols * (0.34 + energy.stereoWidth * 0.10));
    cv::line(layer, {stageCenter.x - speakerOffset, speakerY}, {stageCenter.x + speakerOffset, speakerY}, cv::Scalar(255, 230, 255), 1, cv::LINE_AA);
    for (int side = -1; side <= 1; side += 2) {
        const cv::Point speaker(stageCenter.x + side * speakerOffset, speakerY);
        cv::circle(layer, speaker, static_cast<int>(22 + energy.stereoWidth * 36.0F), cv::Scalar(255, 245, 255), 2, cv::LINE_AA);
        cv::line(layer, speaker, stageCenter, cv::Scalar(225, 195, 255), 1, cv::LINE_AA);
    }

    for (std::size_t index = 0; index < energy.stems.size(); ++index) {
        const auto& stem = energy.stems[index];
        const double phase = timeSeconds * (0.8 + index * 0.13) + static_cast<double>(index) * 1.73;
        const cv::Point point = projectStemPoint(stem, frame.size(), phase);
        const double nearScale = 1.22 - std::clamp(static_cast<double>(stem.depth), 0.0, 1.0) * 0.52;
        const int radius = static_cast<int>((20.0 + stem.energy * 68.0) * nearScale);
        const cv::Scalar color = stemColor(stem);
        const cv::Point floorPoint(point.x, static_cast<int>(stageCenter.y + stem.depth * frame.rows * 0.12));
        cv::line(layer, stageCenter, floorPoint, color, 1 + static_cast<int>(stem.presence * 3.0F), cv::LINE_AA);
        cv::line(layer, floorPoint, point, color, 1 + static_cast<int>(stem.presence * 4.0F), cv::LINE_AA);
        cv::ellipse(layer, floorPoint, {std::max(8, radius), std::max(3, radius / 4)}, 0.0, 0.0, 360.0, color, 1, cv::LINE_AA);
        cv::circle(layer, point, radius + 20, color, 1, cv::LINE_AA);
        cv::circle(layer, point, radius, color, cv::FILLED, cv::LINE_AA);
        cv::circle(layer, point, std::max(4, radius / 3), cv::Scalar(255, 255, 255), cv::FILLED, cv::LINE_AA);
        if (stem.width > 0.5F) {
            cv::ellipse(layer, point, {static_cast<int>(radius * (1.0 + stem.width * 1.25F)), std::max(6, radius / 3)}, 0.0, 0.0, 360.0, color, 2, cv::LINE_AA);
        }
    }

    cv::GaussianBlur(layer, layer, {0, 0}, 5.5 + energy.ambience * 7.0);
    cv::addWeighted(layer, 0.42 + energy.rms * 0.20 + energy.stereoWidth * 0.10, frame, 1.0, 0.0, frame);
}

void addChromaticPrism(cv::Mat& frame, float intensity, float mood) {
    if (intensity < 0.22F) {
        return;
    }
    const int shift = std::clamp(static_cast<int>(1.0F + intensity * 5.0F + mood * 1.5F), 1, 6);

    std::vector<cv::Mat> channels;
    cv::split(frame, channels);
    const cv::Mat redTransform = (cv::Mat_<double>(2, 3) << 1.0, 0.0, shift, 0.0, 1.0, -shift * 0.45);
    const cv::Mat blueTransform = (cv::Mat_<double>(2, 3) << 1.0, 0.0, -shift, 0.0, 1.0, shift * 0.45);
    cv::warpAffine(channels[2], channels[2], redTransform, frame.size(), cv::INTER_LINEAR, cv::BORDER_REFLECT101);
    cv::warpAffine(channels[0], channels[0], blueTransform, frame.size(), cv::INTER_LINEAR, cv::BORDER_REFLECT101);
    cv::merge(channels, frame);
}

void drawBeatShockwaves(cv::Mat& frame, const audio::AudioFrameEnergy& energy, double timeSeconds) {
    if (energy.beatPulse < 0.18F && energy.dropIntensity < 0.22F) {
        return;
    }

    cv::Mat layer = cv::Mat::zeros(frame.size(), frame.type());
    const cv::Point center(static_cast<int>(frame.cols * 0.50), static_cast<int>(frame.rows * 0.56));
    const float power = std::max(energy.beatPulse, energy.dropIntensity);
    for (int ring = 0; ring < 5; ++ring) {
        const double phase = std::fmod(timeSeconds * 2.2 + ring * 0.19, 1.0);
        const int radius = static_cast<int>((0.10 + phase * 0.72) * std::min(frame.cols, frame.rows) * (0.62 + power * 0.22));
        const int thickness = 1 + static_cast<int>(power * 5.0F * (1.0 - phase));
        const cv::Scalar color(255, 185 + ring * 12, 235 + energy.colorMood * 20.0F);
        cv::circle(layer, center, radius, color, std::max(1, thickness), cv::LINE_AA);
    }
    cv::GaussianBlur(layer, layer, {0, 0}, 2.0 + power * 3.0);
    cv::addWeighted(layer, 0.22 + power * 0.34, frame, 1.0, 0.0, frame);
}

void drawStemLabels(cv::Mat& frame, const audio::AudioFrameEnergy& energy) {
    if (energy.stems.empty()) {
        return;
    }

    QImage image(frame.data, frame.cols, frame.rows, static_cast<int>(frame.step), QImage::Format_BGR888);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    const QFont font = makeLyricFont(std::max(18, frame.cols / 96), false);
    painter.setFont(font);

    for (std::size_t index = 0; index < energy.stems.size(); ++index) {
        const auto& stem = energy.stems[index];
        if (stem.energy < 0.16F) {
            continue;
        }
        const cv::Point point = projectStemPoint(stem, frame.size(), static_cast<double>(index) * 1.73);
        const QString label = QString::fromUtf8(stem.name.c_str());
        painter.setOpacity(std::clamp(0.18 + stem.energy * 0.58, 0.0, 0.78));
        drawOutlinedText(
            painter,
            label,
            QPointF(point.x + 18.0, point.y - 14.0),
            font,
            QColor(255, 238, 252),
            QColor(34, 20, 34, 190),
            4.0);
    }
    painter.end();
}

void drawStarbursts(cv::Mat& frame, const audio::AudioFrameEnergy& energy, double timeSeconds) {
    if (energy.treble < 0.10F && energy.vocal < 0.12F) {
        return;
    }
    cv::Mat layer = cv::Mat::zeros(frame.size(), frame.type());
    const int count = 10;
    for (int index = 0; index < count; ++index) {
        const double seed = static_cast<double>(index + 1) * 19.19;
        const double rawX = std::sin(seed * 1.73) * 92821.37;
        const double rawY = std::cos(seed * 2.11) * 71933.11;
        const int x = static_cast<int>((rawX - std::floor(rawX)) * frame.cols);
        const int y = static_cast<int>((rawY - std::floor(rawY)) * frame.rows * 0.78 + frame.rows * 0.06);
        const double pulse = 0.5 + 0.5 * std::sin(timeSeconds * (2.4 + index * 0.11) + seed);
        const int radius = static_cast<int>((8.0 + pulse * 22.0) * (0.35 + energy.treble * 0.72 + energy.vocal * 0.35));
        const cv::Scalar color(255, 236, 255);
        cv::line(layer, {x - radius, y}, {x + radius, y}, color, 1, cv::LINE_AA);
        cv::line(layer, {x, y - radius}, {x, y + radius}, color, 1, cv::LINE_AA);
        cv::line(layer, {x - radius / 2, y - radius / 2}, {x + radius / 2, y + radius / 2}, color, 1, cv::LINE_AA);
        cv::line(layer, {x - radius / 2, y + radius / 2}, {x + radius / 2, y - radius / 2}, color, 1, cv::LINE_AA);
    }
    cv::GaussianBlur(layer, layer, {0, 0}, 1.5 + energy.treble * 2.5);
    cv::addWeighted(layer, 0.28 + energy.treble * 0.38, frame, 1.0, 0.0, frame);
}

void drawDiagonalImpactText(cv::Mat& frame, const std::vector<lyrics::LyricLine>& lines, double timeSeconds, float energy) {
    const auto* line = activeLyric(lines, timeSeconds);
    if (line == nullptr || energy < 0.72F) {
        return;
    }

    QImage image(frame.data, frame.cols, frame.rows, static_cast<int>(frame.step), QImage::Format_BGR888);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.translate(frame.cols * 0.64, frame.rows * 0.16);
    painter.rotate(-8.0 - energy * 3.0);
    painter.setOpacity(std::clamp(0.06 + static_cast<double>(energy) * 0.16, 0.0, 0.26));

    QFont font = makeLyricFont(static_cast<int>(52 + energy * 14.0F), true);
    drawOutlinedText(
        painter,
        QString::fromUtf8(line->text.c_str()),
        QPointF(0, 0),
        font,
        QColor(255, 255, 255),
        QColor(45, 35, 45, 210),
        10.0);
    painter.end();
}

void drawLyrics(cv::Mat& frame, const std::vector<lyrics::LyricLine>& lines, double timeSeconds, float energy) {
    if (lines.empty()) {
        return;
    }

    QImage image(frame.data, frame.cols, frame.rows, static_cast<int>(frame.step), QImage::Format_BGR888);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const int index = activeLyricIndex(lines, timeSeconds);
    if (index < 0) {
        painter.end();
        return;
    }

    const QRectF panel(frame.cols * 0.52, frame.rows * 0.10, frame.cols * 0.42, frame.rows * 0.80);
    const qreal lineSpacing = panel.height() / 9.2;
    const int visibleRadius = 5;
    const auto& activeLine = lines[static_cast<std::size_t>(index)];
    const double activeProgress = std::clamp((timeSeconds - activeLine.startSeconds) / std::max(0.12, activeLine.endSeconds - activeLine.startSeconds), 0.0, 1.0);
    const double scrollT = std::clamp((activeProgress - 0.68) / 0.32, 0.0, 1.0);
    const double smoothScroll = scrollT * scrollT * (3.0 - 2.0 * scrollT);
    const auto drawSlot = [&](int lineIndex, int offset) {
        if (lineIndex < 0 || lineIndex >= static_cast<int>(lines.size())) {
            return;
        }
        const bool active = offset == 0;
        const bool previousActive = offset == -1 && activeProgress < 0.26;
        const bool nextActive = offset == 1 && activeProgress > 0.74;
        const auto& lyric = lines[static_cast<std::size_t>(lineIndex)];
        const double lineProgress = active ? std::clamp((timeSeconds - lyric.startSeconds) / std::max(0.12, lyric.endSeconds - lyric.startSeconds), 0.0, 1.0) : 0.5;
        const qreal activeLift = active ? std::sin(lineProgress * M_PI) * lineSpacing * 0.10 : (previousActive ? lineSpacing * 0.16 : (nextActive ? -lineSpacing * 0.12 : 0.0));
        const qreal centerY = panel.center().y() + (static_cast<qreal>(offset) - smoothScroll) * lineSpacing - activeLift;
        if (centerY < panel.top() - lineSpacing || centerY > panel.bottom() + lineSpacing) {
            return;
        }
        const qreal fade = std::clamp(1.0 - std::abs(offset) / static_cast<qreal>(visibleRadius + 1), 0.0, 1.0);
        const qreal switchBoost = (previousActive || nextActive) ? 0.22 : 0.0;
        const qreal opacity = active ? std::clamp(0.72 + std::sin(lineProgress * M_PI) * 0.28 + energy * 0.08, 0.0, 1.0) : std::clamp(fade * 0.62 + switchBoost, 0.0, 0.88);
        const QString text = QString::fromUtf8(lyric.text.c_str());
        const QRectF area(panel.left(), centerY - lineSpacing * 0.45, panel.width(), lineSpacing * 0.90);
        painter.setOpacity(opacity);
        drawAnimatedLyricText(
            painter,
            text,
            area,
            active ? static_cast<int>(54 + energy * 7.0F) : 42,
            active ? 38 : 30,
            active,
            previousActive,
            nextActive,
            QColor(255, 255, 255),
            QColor(20, 24, 30, active ? 178 : 132),
            active ? 4.6 : 3.2,
            lineProgress,
            energy);
    };

    for (int offset = -visibleRadius; offset <= visibleRadius; ++offset) {
        drawSlot(index + offset, offset);
    }

    painter.end();
}

void drawCenterLyric(cv::Mat& frame, const std::vector<lyrics::LyricLine>& lines, double timeSeconds, float energy) {
    const auto* line = activeLyric(lines, timeSeconds);
    if (line == nullptr) {
        return;
    }

    QImage image(frame.data, frame.cols, frame.rows, static_cast<int>(frame.step), QImage::Format_BGR888);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QString text = QString::fromUtf8(line->text.c_str());
    const QFont font = makeLyricFont(static_cast<int>(68 + energy * 18.0F), true);
    const QFontMetricsF metrics(font);
    const QPointF position((frame.cols - metrics.horizontalAdvance(text)) / 2.0, frame.rows * 0.54);
    painter.setOpacity(0.94);
    drawOutlinedText(painter, text, position, font, QColor(255, 232, 248), QColor(44, 28, 42, 230), 10.0);
    painter.end();
}

void drawKaraokeLyric(cv::Mat& frame, const std::vector<lyrics::LyricLine>& lines, double timeSeconds, float energy) {
    const auto* line = activeLyric(lines, timeSeconds);
    if (line == nullptr) {
        return;
    }

    QImage image(frame.data, frame.cols, frame.rows, static_cast<int>(frame.step), QImage::Format_BGR888);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QString text = QString::fromUtf8(line->text.c_str());
    const QFont font = makeLyricFont(static_cast<int>(48 + energy * 8.0F), true);
    const QFontMetricsF metrics(font);
    const double x = (frame.cols - metrics.horizontalAdvance(text)) / 2.0;
    const double y = frame.rows * 0.86;
    const double progress = std::clamp((timeSeconds - line->startSeconds) / std::max(0.1, line->endSeconds - line->startSeconds), 0.0, 1.0);

    painter.setOpacity(0.96);
    drawOutlinedText(painter, text, QPointF(x, y), font, QColor(245, 245, 250), QColor(35, 24, 34, 220), 8.0);
    painter.save();
    painter.setClipRect(QRectF(x - 8.0, y - metrics.ascent() - 8.0, metrics.horizontalAdvance(text) * progress + 16.0, metrics.height() + 18.0));
    drawOutlinedText(painter, text, QPointF(x, y), font, QColor(255, 176, 231), QColor(78, 22, 62, 220), 8.0);
    painter.restore();
    painter.end();
}

void drawLyricsByLayout(cv::Mat& frame, const std::vector<lyrics::LyricLine>& lines, double timeSeconds, float energy, int layoutMode) {
    switch (layoutMode) {
    case 1:
        drawCenterLyric(frame, lines, timeSeconds, energy);
        break;
    case 2:
        drawKaraokeLyric(frame, lines, timeSeconds, energy);
        break;
    default:
        drawLyrics(frame, lines, timeSeconds, energy);
        break;
    }
}

void addBloom(cv::Mat& frame, double strength) {
    cv::Mat blurred;
    cv::GaussianBlur(frame, blurred, {0, 0}, 18.0);
    cv::addWeighted(frame, 1.0, blurred, strength, 0.0, frame);
}

void applyDreamGrade(cv::Mat& frame, double timeSeconds, float energy) {
    cv::Mat overlay(frame.size(), frame.type());
    for (int y = 0; y < frame.rows; ++y) {
        const double vertical = static_cast<double>(y) / static_cast<double>(std::max(1, frame.rows - 1));
        const cv::Scalar color(
            248.0 + 7.0 * std::sin(timeSeconds * 0.35),
            214.0 + 18.0 * vertical,
            255.0 - 28.0 * vertical);
        overlay.row(y).setTo(color);
    }
    cv::addWeighted(overlay, 0.12 + energy * 0.045, frame, 0.88 - energy * 0.045, 0.0, frame);

    cv::Mat vignette(frame.rows, frame.cols, CV_32F);
    const double cx = frame.cols * 0.5;
    const double cy = frame.rows * 0.52;
    const double maxDistance = std::sqrt(cx * cx + cy * cy);
    for (int y = 0; y < frame.rows; ++y) {
        float* row = vignette.ptr<float>(y);
        for (int x = 0; x < frame.cols; ++x) {
            const double dx = static_cast<double>(x) - cx;
            const double dy = static_cast<double>(y) - cy;
            const double normalized = std::sqrt(dx * dx + dy * dy) / maxDistance;
            row[x] = static_cast<float>(std::clamp(1.14 - normalized * 0.44, 0.72, 1.08));
        }
    }
    std::vector<cv::Mat> channels;
    cv::split(frame, channels);
    for (cv::Mat& channel : channels) {
        cv::Mat channelFloat;
        channel.convertTo(channelFloat, CV_32F);
        cv::multiply(channelFloat, vignette, channelFloat);
        channelFloat.convertTo(channel, CV_8U);
    }
    cv::merge(channels, frame);
}

void applyNeuralColorGrade(cv::Mat& frame, const audio::AudioFrameEnergy& energy, double timeSeconds) {
    cv::Mat overlay(frame.size(), frame.type());
    const cv::Scalar lowMood(255, 208, 235);
    const cv::Scalar highMood(255, 232, 170);
    const double mix = std::clamp(static_cast<double>(energy.colorMood), 0.0, 1.0);
    const cv::Scalar color = lowMood * (1.0 - mix) + highMood * mix;
    overlay.setTo(color);
    const double alpha = 0.035 + energy.vocal * 0.030 + energy.ambience * 0.028 + std::sin(timeSeconds * 0.7) * 0.008;
    cv::addWeighted(overlay, alpha, frame, 1.0 - alpha, 0.0, frame);
}

void applyDepthOfField(cv::Mat& frame, const audio::AudioFrameEnergy& energy) {
    cv::Mat blurred;
    cv::GaussianBlur(frame, blurred, {0, 0}, 3.0 + energy.ambience * 5.0);
    cv::Mat mask(frame.rows, frame.cols, CV_32F);
    const double cx = frame.cols * 0.50;
    const double cy = frame.rows * 0.56;
    const double focusRadius = std::min(frame.cols, frame.rows) * (0.24 + energy.vocal * 0.07);
    for (int y = 0; y < frame.rows; ++y) {
        float* row = mask.ptr<float>(y);
        for (int x = 0; x < frame.cols; ++x) {
            const double dx = static_cast<double>(x) - cx;
            const double dy = static_cast<double>(y) - cy;
            const double distance = std::sqrt(dx * dx + dy * dy);
            row[x] = static_cast<float>(std::clamp((distance - focusRadius) / (focusRadius * 1.9), 0.0, 0.58 + energy.ambience * 0.22));
        }
    }
    cv::Mat frameFloat;
    cv::Mat blurredFloat;
    frame.convertTo(frameFloat, CV_32FC3);
    blurred.convertTo(blurredFloat, CV_32FC3);
    std::vector<cv::Mat> maskChannels(3, mask);
    cv::Mat mask3;
    cv::merge(maskChannels, mask3);
    cv::multiply(blurredFloat, mask3, blurredFloat);
    cv::multiply(frameFloat, cv::Scalar(1.0, 1.0, 1.0) - mask3, frameFloat);
    cv::add(frameFloat, blurredFloat, frameFloat);
    frameFloat.convertTo(frame, CV_8UC3);
}

void applyScanlines(cv::Mat& frame, const audio::AudioFrameEnergy& energy, double timeSeconds) {
    const double strength = 0.006 + energy.dropIntensity * 0.012;
    for (int y = 0; y < frame.rows; y += 3) {
        const double wave = 0.5 + 0.5 * std::sin(timeSeconds * 8.0 + y * 0.035);
        cv::Mat row = frame.row(y);
        cv::addWeighted(row, 1.0 - strength * wave, cv::Mat(row.size(), row.type(), cv::Scalar(18, 10, 22)), strength * wave, 0.0, row);
    }
}

std::string shellQuote(const std::filesystem::path& path) {
    std::string text = path.string();
    std::string quoted = "'";
    for (const char ch : text) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
}

bool commandSucceeds(const std::string& command) {
    return std::system(command.c_str()) == 0;
}

std::string shellQuoteText(const std::string& text) {
    std::string quoted = "'";
    for (const char ch : text) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
}

bool ffmpegHasEncoder(const std::string& encoderName) {
    std::ostringstream command;
    command << "ffmpeg -hide_banner -loglevel error -encoders 2>/dev/null | grep -q " << encoderName;
    return commandSucceeds(command.str());
}

bool cudaRuntimeAvailable() {
    return commandSucceeds("ldconfig -p 2>/dev/null | grep -q 'libcuda.so.1'") || std::filesystem::exists("/usr/lib/x86_64-linux-gnu/libcuda.so.1");
}

bool startsWith(const std::string& text, const std::string& prefix) {
    return text.rfind(prefix, 0) == 0;
}

int effectiveRenderThreads(const RenderConfig& config) {
    const int hardwareThreads = static_cast<int>(std::max(1U, std::thread::hardware_concurrency()));
    if (config.renderThreads <= 0) {
        return std::clamp(hardwareThreads, 1, 48);
    }
    return std::clamp(config.renderThreads, 1, 96);
}

std::string selectedVaapiDevice(const RenderConfig& config) {
    if (startsWith(config.encoderDevice, "vaapi:")) {
        return config.encoderDevice.substr(std::string("vaapi:").size());
    }
    if (std::filesystem::exists("/dev/dri/renderD128")) {
        return "/dev/dri/renderD128";
    }
    if (std::filesystem::exists("/dev/dri/renderD129")) {
        return "/dev/dri/renderD129";
    }
    return "/dev/dri/renderD128";
}

int selectedCudaDevice(const RenderConfig& config) {
    if (!startsWith(config.encoderDevice, "cuda:")) {
        return 0;
    }
    try {
        return std::max(0, std::stoi(config.encoderDevice.substr(std::string("cuda:").size())));
    } catch (...) {
        return 0;
    }
}

std::string safeLibx264Preset(const std::string& requestedPreset) {
    static const std::array<std::string, 10> validPresets = {
        "ultrafast", "superfast", "veryfast", "faster", "fast",
        "medium", "slow", "slower", "veryslow", "placebo"
    };
    if (std::find(validPresets.begin(), validPresets.end(), requestedPreset) != validPresets.end()) {
        return requestedPreset;
    }
    return "veryfast";
}

std::string safeNvencPreset(const std::string& requestedPreset) {
    if (requestedPreset.size() == 2 && requestedPreset[0] == 'p' && requestedPreset[1] >= '1' && requestedPreset[1] <= '7') {
        return requestedPreset;
    }
    return "p5";
}

bool encoderPreflightSucceeds(const RenderConfig& config, const std::string& backend) {
    std::ostringstream command;
    command << "ffmpeg -y -hide_banner -loglevel error "
            << "-f lavfi -i color=c=black:s=64x64:r=1:d=0.12 "
            << "-frames:v 1 -an ";
    const int cq = std::clamp(config.encoderCrf, 10, 28);
    if (backend == "h264_nvenc") {
        if (!cudaRuntimeAvailable()) {
            return false;
        }
        command << "-c:v h264_nvenc -gpu " << selectedCudaDevice(config) << " -preset " << safeNvencPreset(config.encoderPreset) << " -tune hq -rc vbr -cq " << cq << " -bf 3 -spatial-aq 1 -temporal-aq 1 -rc-lookahead 20 -surfaces 32 ";
    } else if (backend == "h264_vaapi") {
        const std::string device = selectedVaapiDevice(config);
        if (!std::filesystem::exists(device)) {
            return false;
        }
        command << "-vaapi_device " << shellQuote(device) << " -vf format=nv12,hwupload -c:v h264_vaapi -qp " << cq << ' ';
    } else {
        command << "-c:v libx264 -preset " << safeLibx264Preset(config.encoderPreset) << " -crf 28 -pix_fmt yuv420p ";
    }
    command << "-f null - >/dev/null 2>&1";
    return commandSucceeds(command.str());
}

std::string resolveEncoderBackend(const RenderConfig& config) {
    const std::string requested = config.encoderBackend.empty() ? "auto" : config.encoderBackend;
    if (requested != "auto") {
        if (encoderPreflightSucceeds(config, requested)) {
            return requested;
        }
        if (requested != "libx264" && encoderPreflightSucceeds(config, "libx264")) {
            return "libx264";
        }
        return "libx264";
    }

    if ((config.encoderDevice == "auto" || startsWith(config.encoderDevice, "cuda:")) && ffmpegHasEncoder("h264_nvenc") && encoderPreflightSucceeds(config, "h264_nvenc")) {
        return "h264_nvenc";
    }
    if ((config.encoderDevice == "auto" || startsWith(config.encoderDevice, "vaapi:")) && ffmpegHasEncoder("h264_vaapi") && encoderPreflightSucceeds(config, "h264_vaapi")) {
        return "h264_vaapi";
    }
    return "libx264";
}

void muxAudioWithFfmpeg(const std::filesystem::path& videoOnlyPath, const std::filesystem::path& musicPath, const std::filesystem::path& outputPath) {
    const std::string surroundFilter =
        "[1:a]aformat=sample_fmts=fltp:sample_rates=48000:channel_layouts=stereo,"
        "highpass=f=24,lowpass=f=20500,"
        "equalizer=f=62:t=q:w=1.05:g=1.4,"
        "equalizer=f=230:t=q:w=1.20:g=-0.8,"
        "equalizer=f=3200:t=q:w=1.10:g=0.9,"
        "equalizer=f=9800:t=q:w=0.85:g=1.2,"
        "dynaudnorm=f=250:g=9:p=0.62:m=12:s=18,"
        "asplit=5[dry][wide][air][bass][glue];"
        "[wide]highpass=f=145,lowpass=f=13500,"
        "adelay=0|18,"
        "aecho=0.30:0.42:28|56|92:0.12|0.08|0.045,"
        "aphaser=in_gain=0.62:out_gain=0.74:delay=2.1:decay=0.30:speed=0.24,"
        "pan=stereo|c0=0.92*c0+0.16*c1|c1=0.16*c0+0.92*c1[wide3d];"
        "[air]highpass=f=3600,adelay=11|0,"
        "aecho=0.22:0.36:46|88|132:0.08|0.055|0.035,"
        "volume=0.72[air3d];"
        "[bass]lowpass=f=135,"
        "pan=stereo|c0=0.70*c0+0.30*c1|c1=0.30*c0+0.70*c1,"
        "volume=0.92[basscenter];"
        "[glue]bandpass=f=950:w=3600,acompressor=threshold=0.18:ratio=1.45:attack=18:release=160:makeup=1.35[body];"
        "[dry][wide3d][air3d][basscenter][body]amix=inputs=5:weights=0.82 0.34 0.16 0.30 0.18:normalize=0,"
        "acompressor=threshold=0.72:ratio=1.35:attack=8:release=120:makeup=1.03,"
        "alimiter=limit=0.965:level=disabled,"
        "aresample=48000:resampler=soxr:precision=28[aout]";

    std::ostringstream command;
    command << "ffmpeg -y -hide_banner -loglevel error "
            << "-i " << shellQuote(videoOnlyPath) << ' '
            << "-i " << shellQuote(musicPath) << ' '
            << "-filter_complex " << shellQuoteText(surroundFilter) << ' '
            << "-map 0:v:0 -map [aout] "
            << "-c:v copy -c:a aac -b:a 512k -ar 48000 -shortest -movflags +faststart "
            << shellQuote(outputPath);

    const int exitCode = std::system(command.str().c_str());
    if (exitCode == 0) {
        return;
    }

    std::ostringstream fallbackCommand;
    fallbackCommand << "ffmpeg -y -hide_banner -loglevel error "
                    << "-i " << shellQuote(videoOnlyPath) << ' '
                    << "-i " << shellQuote(musicPath) << ' '
                    << "-map 0:v:0 -map 1:a:0 "
                    << "-c:v copy -c:a aac -b:a 512k -ar 48000 -shortest -movflags +faststart "
                    << shellQuote(outputPath);

    if (std::system(fallbackCommand.str().c_str()) != 0) {
        throw std::runtime_error("FFmpeg 音轨合并失败，请确认已安装 ffmpeg 命令行工具");
    }
}

std::string makeRawVideoPipeCommand(const RenderConfig& config, const std::filesystem::path& videoOnlyPath) {
    std::ostringstream command;
    const int safeCrf = std::clamp(config.encoderCrf, 10, 28);
    const int cq = std::clamp(config.encoderCrf, 10, 28);
    const std::string backend = resolveEncoderBackend(config);
    command << "ffmpeg -y -hide_banner -loglevel error "
            << "-f rawvideo -pix_fmt bgr24 "
            << "-s " << config.width << 'x' << config.height << ' '
            << "-r " << config.fps << ' '
            << "-i - -an ";
    if (backend == "h264_nvenc") {
        command << "-c:v h264_nvenc -gpu " << selectedCudaDevice(config) << " -preset " << safeNvencPreset(config.encoderPreset) << " -tune hq -rc vbr -cq " << cq << " -bf 3 -spatial-aq 1 -temporal-aq 1 -rc-lookahead 20 -surfaces 32 -pix_fmt yuv420p -movflags +faststart ";
    } else if (backend == "h264_vaapi") {
        command << "-vaapi_device " << shellQuote(selectedVaapiDevice(config)) << " -vf format=nv12,hwupload -c:v h264_vaapi -qp " << cq << " -movflags +faststart ";
    } else {
        command << "-c:v libx264 -preset " << safeLibx264Preset(config.encoderPreset) << " -crf " << safeCrf << " -pix_fmt yuv420p -movflags +faststart ";
    }
    command << shellQuote(videoOnlyPath);
    return command.str();
}

cv::Mat composeFrame(
    const RenderConfig& config,
    const cv::Mat& base,
    const cv::Mat& circleCover,
    const cv::Scalar& accent,
    const cv::Mat& coolWash,
    const cv::Mat& airyShadow,
    const std::vector<lyrics::LyricLine>& lyricLines,
    const audio::AudioFrameEnergy& energy,
    double timeSeconds,
    int frameIndex,
    int totalFrames,
    bool fastPreview = false) {
    const double dx = std::sin(timeSeconds * 0.22) * config.parallaxStrength * 0.16;
    const double dy = std::cos(timeSeconds * 0.18) * config.parallaxStrength * 0.10;
    const double roll = std::sin(timeSeconds * 0.16) * (fastPreview ? 0.08 : 0.18);
    const double scale = 1.025 + energy.bass * config.pulseStrength * (fastPreview ? 0.24 : 0.42);
    const cv::Point2f center(static_cast<float>(config.width) * 0.5F, static_cast<float>(config.height) * 0.5F);
    cv::Mat transform = cv::getRotationMatrix2D(center, roll, scale);
    transform.at<double>(0, 2) += dx;
    transform.at<double>(1, 2) += dy;

    cv::Mat frame;
    cv::warpAffine(base, frame, transform, base.size(), fastPreview ? cv::INTER_LINEAR : cv::INTER_CUBIC, cv::BORDER_REFLECT101);

    cv::addWeighted(coolWash, fastPreview ? 0.13 : 0.18, frame, fastPreview ? 0.87 : 0.82, 0.0, frame);
    cv::addWeighted(airyShadow, fastPreview ? 0.10 : 0.16, frame, fastPreview ? 0.90 : 0.84, 0.0, frame);
    if (!fastPreview) {
        drawSoftSnow(frame, timeSeconds, cv::Scalar(255, 255, 255));
        drawAuroraRibbons(frame, energy, timeSeconds);
        drawSurroundStage(frame, energy, timeSeconds);
    }
    drawSongInfo(frame, config, accent, timeSeconds);
    drawWatermark(frame, config);

    const int coverDiameter = static_cast<int>(std::min(config.width, config.height) * 0.28);
    const cv::Point coverCenter(static_cast<int>(config.width * 0.27), static_cast<int>(config.height * 0.57));
    const int blackRingRadius = coverDiameter / 2 + static_cast<int>(std::min(config.width, config.height) * 0.036);
    const int spectrumRadius = blackRingRadius + static_cast<int>(std::min(config.width, config.height) * 0.022);
    if (!fastPreview) {
        drawBassHalo(frame, energy, coverCenter, spectrumRadius);
    }
    drawSpectrumRing(frame, energy.spectrumBins, coverCenter, spectrumRadius, config.spectrumBarHeight * (fastPreview ? 0.40 : 0.58), cv::Scalar(255, 255, 255));

    if (config.enableCircularCover) {
        cv::circle(frame, coverCenter, blackRingRadius, cv::Scalar(0, 0, 0), cv::FILLED, cv::LINE_AA);
        alphaBlendCircle(frame, circleCover, {coverCenter.x - coverDiameter / 2, coverCenter.y - coverDiameter / 2}, 0.96);
        cv::circle(frame, coverCenter, coverDiameter / 2 + 2, cv::Scalar(12, 12, 12), 3, cv::LINE_AA);
    }

    drawLyricsByLayout(frame, lyricLines, timeSeconds, energy.rms, config.lyricLayoutMode);
    if (!fastPreview) {
        if (config.enableParticles) {
            drawParticles(frame, timeSeconds, std::max(energy.rms, energy.beatPulse));
        }
        drawAudioComets(frame, energy, timeSeconds);
        drawBeatShockwaves(frame, energy, timeSeconds);
        drawStarbursts(frame, energy, timeSeconds);
        if (config.enableImpactFlash) {
            addImpactFlash(frame, energy.dropIntensity);
        }
        if (config.enableBloom) {
            addBloom(frame, 0.045 + energy.ambience * 0.030 + energy.beatPulse * 0.050);
        }
        applyNeuralColorGrade(frame, energy, timeSeconds);
        applyDreamGrade(frame, timeSeconds, std::max(energy.rms, energy.dropIntensity));
    } else {
        applyNeuralColorGrade(frame, energy, timeSeconds);
    }

    const int progressY = std::max(0, config.height - 5);
    cv::line(frame, {0, progressY}, {static_cast<int>(config.width * (static_cast<double>(frameIndex) / totalFrames)), progressY}, cv::Scalar(188, 112, 255), 5, cv::LINE_AA);

    return frame;
}

} // namespace

VideoRenderer::VideoRenderer() = default;
VideoRenderer::~VideoRenderer() = default;

void VideoRenderer::render(const RenderConfig& config, const ProgressCallback& onProgress, const CancelCallback& shouldCancel, const PreviewFrameCallback& onPreviewFrame) const {
    if (config.backgroundImagePath.empty() || config.musicPath.empty() || config.lyricPath.empty() || config.outputVideoPath.empty()) {
        throw std::runtime_error("背景、音乐、歌词、输出路径都必须选择");
    }

    std::signal(SIGPIPE, SIG_IGN);

    cv::Mat background = cv::imread(config.backgroundImagePath.string(), cv::IMREAD_COLOR);
    if (background.empty()) {
        throw std::runtime_error("无法读取背景图片: " + config.backgroundImagePath.string());
    }

    audio::AudioAnalyzer analyzer;
    const audio::AudioAnalysisResult analysis = analyzer.analyze(config.musicPath, config.fps);

    lyrics::LrcParser parser;
    const std::vector<lyrics::LyricLine> lyricLines = parser.parse(config.lyricPath);

    const int totalFrames = std::max(1, static_cast<int>(std::ceil(analysis.durationSeconds * config.fps)));
    const std::filesystem::path outputPath = config.outputVideoPath;
    const std::filesystem::path videoOnlyPath = outputPath.parent_path() / (outputPath.stem().string() + ".video_only.mp4");

    FILE* encoderPipe = popen(makeRawVideoPipeCommand(config, videoOnlyPath).c_str(), "w");
    if (encoderPipe == nullptr) {
        throw std::runtime_error("无法启动 FFmpeg 管线编码器，请确认已安装 ffmpeg 命令行工具");
    }

    const cv::Mat base = coverResize(background, config.width, config.height);
    const cv::Scalar accent = estimateLightAccentColor(base);
    const cv::Mat coolWash(base.size(), base.type(), cv::Scalar(accent[0], accent[1], accent[2]));
    const cv::Mat airyShadow(base.size(), base.type(), cv::Scalar(58, 62, 70));
    const int coverDiameter = static_cast<int>(std::min(config.width, config.height) * 0.28);
    const cv::Mat circleCover = makeCircularImage(background, coverDiameter);
    const int renderThreads = effectiveRenderThreads(config);
    const std::size_t bytesPerFrame = static_cast<std::size_t>(config.width) * static_cast<std::size_t>(config.height) * 3U;
    const int memorySafeBatch = static_cast<int>(std::clamp<std::size_t>((512U * 1024U * 1024U) / std::max<std::size_t>(1U, bytesPerFrame), 1U, 192U));
    const int requestedBatch = config.renderBatchFrames > 0 ? config.renderBatchFrames : renderThreads * 3;
    const int batchSize = std::max(1, std::min(requestedBatch, memorySafeBatch));

    if (onProgress) {
        RenderProgress setupProgress;
        setupProgress.currentFrame = 0;
        setupProgress.totalFrames = totalFrames;
        setupProgress.progress = 0.0;
        const auto gpuStatus = gpu::queryWebGpuBackend(config);
        setupProgress.message = gpuStatus.message;
        onProgress(setupProgress);
    }

    for (int batchStart = 0; batchStart < totalFrames; batchStart += batchSize) {
        if (shouldCancel && shouldCancel()) {
            pclose(encoderPipe);
            std::error_code removeError;
            std::filesystem::remove(videoOnlyPath, removeError);
            throw std::runtime_error("渲染已由用户取消");
        }

        const int batchEnd = std::min(totalFrames, batchStart + batchSize);
        const int currentBatchSize = batchEnd - batchStart;
        std::vector<cv::Mat> frames(static_cast<std::size_t>(currentBatchSize));

#if LIREAL_HAS_OPENMP
#pragma omp parallel for schedule(dynamic) num_threads(renderThreads)
#endif
        for (int localIndex = 0; localIndex < currentBatchSize; ++localIndex) {
            const int frameIndex = batchStart + localIndex;
            const double t = static_cast<double>(frameIndex) / static_cast<double>(config.fps);
            const auto& energy = analysis.frames[std::min<std::size_t>(frameIndex, analysis.frames.size() - 1)];
            frames[static_cast<std::size_t>(localIndex)] = composeFrame(config, base, circleCover, accent, coolWash, airyShadow, lyricLines, energy, t, frameIndex, totalFrames);
        }

        for (int localIndex = 0; localIndex < currentBatchSize; ++localIndex) {
            if (shouldCancel && shouldCancel()) {
                pclose(encoderPipe);
                std::error_code removeError;
                std::filesystem::remove(videoOnlyPath, removeError);
                throw std::runtime_error("渲染已由用户取消");
            }

            const int frameIndex = batchStart + localIndex;
            cv::Mat frame = frames[static_cast<std::size_t>(localIndex)];
            const int previewStride = std::max(1, config.fps / 3);
            if (onPreviewFrame && (frameIndex % previewStride == 0 || frameIndex + 1 == totalFrames)) {
                cv::Mat previewFrame = frame;
                const int maxPreviewWidth = std::clamp(config.previewMaxWidth, 480, 1920);
                const int maxPreviewHeight = std::clamp(config.previewMaxHeight, 270, 1080);
                const double previewScale = std::min({1.0, maxPreviewWidth / static_cast<double>(std::max(1, frame.cols)), maxPreviewHeight / static_cast<double>(std::max(1, frame.rows))});
                if (previewScale < 0.999) {
                    cv::resize(frame, previewFrame, {}, previewScale, previewScale, cv::INTER_AREA);
                }
                QImage image(previewFrame.data, previewFrame.cols, previewFrame.rows, static_cast<int>(previewFrame.step), QImage::Format_BGR888);
                const auto& energy = analysis.frames[std::min<std::size_t>(frameIndex, analysis.frames.size() - 1)];
                onPreviewFrame(image.copy(), frameIndex + 1, totalFrames, energy);
            }
            if (!frame.isContinuous()) {
                frame = frame.clone();
            }
            const std::size_t bytesToWrite = static_cast<std::size_t>(frame.total()) * static_cast<std::size_t>(frame.elemSize());
            const std::size_t bytesWritten = fwrite(frame.data, 1U, bytesToWrite, encoderPipe);
            if (bytesWritten != bytesToWrite) {
                pclose(encoderPipe);
                std::error_code removeError;
                std::filesystem::remove(videoOnlyPath, removeError);
                throw std::runtime_error("FFmpeg 管线写入失败，视频编码中断");
            }

            if (onProgress && (frameIndex % std::max(1, config.fps / 4) == 0 || frameIndex + 1 == totalFrames)) {
                onProgress({frameIndex + 1, totalFrames, static_cast<double>(frameIndex + 1) / static_cast<double>(totalFrames), "多线程合成并编码音画帧"});
            }
        }
    }

    if (pclose(encoderPipe) != 0) {
        std::error_code removeError;
        std::filesystem::remove(videoOnlyPath, removeError);
        throw std::runtime_error("FFmpeg 视频编码失败");
    }

    if (onProgress) {
        onProgress({totalFrames, totalFrames, 0.985, "正在合并 HiFi 双声道 3D 环绕音轨"});
    }

    if (shouldCancel && shouldCancel()) {
        std::error_code removeError;
        std::filesystem::remove(videoOnlyPath, removeError);
        throw std::runtime_error("渲染已由用户取消");
    }

    muxAudioWithFfmpeg(videoOnlyPath, config.musicPath, outputPath);
    std::error_code removeError;
    std::filesystem::remove(videoOnlyPath, removeError);

    if (onProgress) {
        onProgress({totalFrames, totalFrames, 1.0, "渲染完成"});
    }
}

void VideoRenderer::renderPreviewImage(const RenderConfig& config, const std::filesystem::path& previewPath, double preferredTimeSeconds) const {
    const RenderConfig previewConfig = makeFastPreviewConfig(config);
    if (previewConfig.backgroundImagePath.empty() || previewConfig.musicPath.empty() || previewConfig.lyricPath.empty()) {
        throw std::runtime_error("背景、音乐、歌词都必须选择后才能生成预览图");
    }

    cv::Mat background = cv::imread(previewConfig.backgroundImagePath.string(), cv::IMREAD_COLOR);
    if (background.empty()) {
        throw std::runtime_error("无法读取背景图片: " + previewConfig.backgroundImagePath.string());
    }

    audio::AudioAnalyzer analyzer;
    const audio::AudioAnalysisResult analysis = analyzer.analyze(previewConfig.musicPath, previewConfig.fps);
    if (analysis.frames.empty()) {
        throw std::runtime_error("无法从音乐中生成预览驱动数据");
    }

    lyrics::LrcParser parser;
    const std::vector<lyrics::LyricLine> lyricLines = parser.parse(previewConfig.lyricPath);

    const int totalFrames = std::max(1, static_cast<int>(std::ceil(analysis.durationSeconds * previewConfig.fps)));
    const double previewTime = std::clamp(preferredTimeSeconds, 0.0, std::max(0.0, analysis.durationSeconds - 0.05));
    const int frameIndex = std::clamp(static_cast<int>(previewTime * previewConfig.fps), 0, totalFrames - 1);

    const cv::Mat base = coverResize(background, previewConfig.width, previewConfig.height);
    const cv::Scalar accent = estimateLightAccentColor(base);
    const cv::Mat coolWash(base.size(), base.type(), cv::Scalar(accent[0], accent[1], accent[2]));
    const cv::Mat airyShadow(base.size(), base.type(), cv::Scalar(58, 62, 70));
    const int coverDiameter = static_cast<int>(std::min(previewConfig.width, previewConfig.height) * 0.28);
    const cv::Mat circleCover = makeCircularImage(background, coverDiameter);
    const auto& energy = analysis.frames[std::min<std::size_t>(frameIndex, analysis.frames.size() - 1)];
    cv::Mat frame = composeFrame(previewConfig, base, circleCover, accent, coolWash, airyShadow, lyricLines, energy, previewTime, frameIndex, totalFrames, previewConfig.enableFastPreview);

    std::filesystem::create_directories(previewPath.parent_path());
    if (!cv::imwrite(previewPath.string(), frame)) {
        throw std::runtime_error("无法写入预览图: " + previewPath.string());
    }
}

void VideoRenderer::renderPreviewStream(const RenderConfig& config, double startSeconds, double durationSeconds, const PreviewFrameCallback& onFrame, const CancelCallback& shouldCancel) const {
    const RenderConfig previewConfig = makeFastPreviewConfig(config);
    if (previewConfig.backgroundImagePath.empty() || previewConfig.musicPath.empty() || previewConfig.lyricPath.empty()) {
        throw std::runtime_error("背景、音乐、歌词都必须选择后才能打开实时预览");
    }

    cv::Mat background = cv::imread(previewConfig.backgroundImagePath.string(), cv::IMREAD_COLOR);
    if (background.empty()) {
        throw std::runtime_error("无法读取背景图片: " + previewConfig.backgroundImagePath.string());
    }

    audio::AudioAnalyzer analyzer;
    const audio::AudioAnalysisResult analysis = analyzer.analyze(previewConfig.musicPath, previewConfig.fps);
    if (analysis.frames.empty()) {
        throw std::runtime_error("无法从音乐中生成预览驱动数据");
    }

    lyrics::LrcParser parser;
    const std::vector<lyrics::LyricLine> lyricLines = parser.parse(previewConfig.lyricPath);

    const cv::Mat base = coverResize(background, previewConfig.width, previewConfig.height);
    const cv::Scalar accent = estimateLightAccentColor(base);
    const cv::Mat coolWash(base.size(), base.type(), cv::Scalar(accent[0], accent[1], accent[2]));
    const cv::Mat airyShadow(base.size(), base.type(), cv::Scalar(58, 62, 70));
    const int coverDiameter = static_cast<int>(std::min(previewConfig.width, previewConfig.height) * 0.28);
    const cv::Mat circleCover = makeCircularImage(background, coverDiameter);
    const int totalFrames = std::max(1, static_cast<int>(std::ceil(analysis.durationSeconds * previewConfig.fps)));
    const double safeStart = std::clamp(startSeconds, 0.0, std::max(0.0, analysis.durationSeconds - 0.05));
    const double safeDuration = std::clamp(durationSeconds, 0.5, std::max(0.5, analysis.durationSeconds - safeStart));
    const int previewFrames = std::max(1, static_cast<int>(std::ceil(safeDuration * previewConfig.fps)));

    for (int previewIndex = 0; previewIndex < previewFrames; ++previewIndex) {
        if (shouldCancel && shouldCancel()) {
            return;
        }
        const double timeSeconds = std::min(analysis.durationSeconds, safeStart + static_cast<double>(previewIndex) / static_cast<double>(previewConfig.fps));
        const int frameIndex = std::clamp(static_cast<int>(timeSeconds * previewConfig.fps), 0, totalFrames - 1);
        const auto& energy = analysis.frames[std::min<std::size_t>(frameIndex, analysis.frames.size() - 1)];
        cv::Mat frame = composeFrame(previewConfig, base, circleCover, accent, coolWash, airyShadow, lyricLines, energy, timeSeconds, frameIndex, totalFrames, previewConfig.enableFastPreview);
        QImage image(frame.data, frame.cols, frame.rows, static_cast<int>(frame.step), QImage::Format_BGR888);
        onFrame(image.copy(), previewIndex + 1, previewFrames, energy);
    }
}

} // namespace lireal::render
