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

#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>

int main(int argc, char* argv[]) {
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Lireal Music"));
    app.setOrganizationName(QStringLiteral("Lireal"));

#ifdef LIREAL_THEME_PATH
    QFile themeFile(QStringLiteral(LIREAL_THEME_PATH));
    if (themeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&themeFile);
        app.setStyleSheet(stream.readAll());
    }
#endif

    lireal::ui::DashboardWindow window;
    window.show();
    return app.exec();
}
