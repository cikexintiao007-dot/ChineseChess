// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GAMETHEME_H
#define GAMETHEME_H

#include <QApplication>
#include <QPalette>
#include <QPixmap>
#include <QFontDatabase>

inline QString gameFontFamily(const QStringList& candidates)
{
    const QStringList available = QFontDatabase().families();
    for (const auto& family : candidates) {
        if (available.contains(family, Qt::CaseInsensitive)) return family;
    }
    return QApplication::font().family();
}

inline QString gameSerifFontFamily()
{
    static const QString family = gameFontFamily({QStringLiteral("KaiTi"), QStringLiteral("STKaiti"),
        QStringLiteral("Noto Serif CJK SC"), QStringLiteral("Source Han Serif SC"),
        QStringLiteral("WenQuanYi Micro Hei"), QStringLiteral("Noto Sans CJK SC")});
    return family;
}

inline QPixmap createSceneBackground(const QSize& size, qreal pixelRatio)
{
    const QSize pixels = size * pixelRatio;
    const QPixmap source(QStringLiteral(":/images/ink-landscape.png"));
    QPixmap result = source.scaled(pixels, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    result.setDevicePixelRatio(pixelRatio);
    return result;
}

inline void applyGameTheme(QApplication& application)
{
    application.setStyle("Fusion");
    const QString sans = gameFontFamily({QStringLiteral("Microsoft YaHei"), QStringLiteral("Noto Sans CJK SC"),
        QStringLiteral("Source Han Sans SC"), QStringLiteral("WenQuanYi Micro Hei")});
    const QString mono = gameFontFamily({QStringLiteral("Consolas"), QStringLiteral("DejaVu Sans Mono"), QStringLiteral("Liberation Mono")});
    application.setFont(QFont(sans, 10));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#f2e7d5"));
    palette.setColor(QPalette::WindowText, QColor("#4c2f1e"));
    palette.setColor(QPalette::Base, QColor("#fff7e9"));
    palette.setColor(QPalette::AlternateBase, QColor("#f6ead7"));
    palette.setColor(QPalette::Text, QColor("#4c2f1e"));
    palette.setColor(QPalette::Button, QColor("#fff7e9"));
    palette.setColor(QPalette::ButtonText, QColor("#4c2f1e"));
    palette.setColor(QPalette::Highlight, QColor("#9b422d"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#9b8a74"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#9b8a74"));
    application.setPalette(palette);
    application.setStyleSheet(QStringLiteral(R"(
        QMainWindow, QDialog { background: #f2e7d5; }
        QLabel { color: #4c2f1e; background: transparent; }
        QLabel#menuTitle { font: 700 26pt '%1'; color: #5a301b; }
        QFrame#gameHeader {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #75492e,stop:1 #47291c);
            border-bottom: 2px solid #b5854d;
        }
        QLabel#gameTitle { font: 700 26px '%1'; color: #ffe4b5; }
        QLabel#labelMode { color: #e9cba3; font-size: 12px; }
        QLabel#headerState {
            color: #f8e5bc; background: #503723; border: 1px solid #9f7a4d;
            border-radius: 9px; padding: 6px 14px; font-size: 13px;
        }
        QFrame#statusCard, QFrame#blackPlayerCard, QFrame#controlCard {
            background: rgba(255,248,235,242); border: 1px solid #bea17a; border-radius: 12px;
        }
        QLabel#labelRedName, QLabel#labelBlackName { font: 700 20px '%1'; }
        QLabel#labelRedName { color: #9c3527; }
        QLabel#labelRedRole, QLabel#labelBlackRole, QLabel#labelSides,
        QLabel#labelPuzzleDescription, QLabel#labelHint { color: #755b42; font-size: 12px; }
        QLabel#capturedByRed { font: 16px '%1'; color: #4c3525; }
        QLabel#capturedByBlack { font: 16px '%1'; color: #9c3527; }
        QLabel#turnLabel {
            color: #fff0d1; background: #a74730; border: 1px solid #8c4d2d;
            border-radius: 8px; padding: 6px 10px; font-weight: 700;
        }
        QLabel#turnLabel[state="black"] { color: #fff0d1; background: #635044; }
        QLabel#turnLabel[state="thinking"] { color: #fff0d1; background: #67704b; }
        QLabel#lcdNumber { font: 22px '%2'; color: #593821; }
        QLabel#elapsedCaption, QLabel#labelMoveCount { color: #755b42; font-size: 12px; }
        QLabel#redMatchClock, QLabel#blackMatchClock {
            background: #eee2ce; color: #4c2f1e; border: 1px solid #cbb395;
            border-radius: 6px; padding: 4px 2px; font: 700 13px '%2';
        }
        QLabel#redMatchClock[clockActive="true"] { background: #9b422d; color: #fff5e0; }
        QLabel#blackMatchClock[clockActive="true"] { background: #554032; color: #fff5e0; }
        QLabel#historyCaption { font: 700 19px '%1'; color: #5a301b; }
        QPushButton {
            color: #4c2f1e;
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #fffaf0,stop:1 #f1dfc3);
            border: 1px solid #bb966b; border-radius: 8px;
            padding: 8px 12px; min-height: 18px;
        }
        QPushButton:hover { color: #743d22; background: #fff0d1; border-color: #966134; }
        QPushButton:pressed { background: #e5c899; border-color: #8a522c; }
        QPushButton:focus { border: 1px solid #8a522c; }
        QPushButton:disabled { color: #a28e75; background: #eee2ce; border-color: #d6c4a9; }
        QPushButton#pushButton_start { padding: 7px 8px; font-size: 12px; }
        QPushButton#pushButton_restart, QPushButton#resultAgain {
            color: #fff0cb; border-color: #8c4b28;
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #ba5837,stop:1 #923527);
            font-size: 17px; font-weight: 600;
        }
        QPushButton#pushButton_restart:hover, QPushButton#resultAgain:hover { background: #a2442b; }
        QLabel#resultWinner { font: 700 34px '%1'; color: #9d3927; }
        QLabel#resultWinner[side="black"] { color: #4e3b2c; }
        QLabel#resultRole { font-size: 14px; color: #6c5035; }
        QLabel#resultMode, QLabel#resultCaption { font-size: 12px; color: #806548; }
        QLabel#resultTime, QLabel#resultSteps { font: 21px '%2'; color: #563821; }
        QFrame#resultStats { background: #f6ead7; border: 1px solid #d5bd99; border-radius: 10px; }
        QPushButton#pushButton_toMenu {
            background: #63412c; color: #ffe4b5; border-color: #a17b50;
        }
        QPushButton#pushButton_toMenu:hover { background: #795238; }
        QDialog#gameMenu > QPushButton {
            font-size: 16px; font-weight: 600; border-radius: 12px;
            padding: 12px 22px; border: 1px solid #bc996d;
        }
        QDialog#gameMenu > QPushButton:focus { border: 1px solid #974a2d; }
        QDialog#gameMenu > QPushButton#menuContinueButton:enabled {
            background: #8e482c; color: #fff7e9; padding: 8px;
        }
        QDialog#gameMenu > QPushButton#menuOpenRecordButton,
        QDialog#gameMenu > QPushButton#menuSetupButton,
        QDialog#gameMenu > QPushButton#menuGuideButton,
        QDialog#gameMenu > QPushButton#menuSettingsButton { font-size: 14px; padding: 8px; }
        QDialog#positionEditor QPushButton[pieceSide] { padding: 5px 7px; font-size: 17px; }
        QDialog#positionEditor QPushButton[pieceSide="red"] { color: #962e24; }
        QDialog#positionEditor QPushButton:checked { background: #eac797; border: 2px solid #8a522c; }
        QDialog#positionEditor QPushButton:checked:focus { border-color: #456643; }
        QDialog#recordLibraryPage QListWidget, QDialog#puzzleSelectionPage QListWidget {
            background: #fff7e9; color: #4c2f1e; border: 1px solid #bea17a;
            border-radius: 8px; padding: 6px;
        }
        QDialog#recordLibraryPage QListWidget::item, QDialog#puzzleSelectionPage QListWidget::item {
            padding: 9px 8px;
        }
        QDialog#recordLibraryPage QListWidget::item:selected, QDialog#puzzleSelectionPage QListWidget::item:selected {
            background: #8e482c; color: #fff7e9;
        }
        QCheckBox { color: #664a32; spacing: 8px; font-size: 12px; }
        QCheckBox::indicator { width: 16px; height: 16px; }
        QListWidget#moveList {
            background: rgba(255,249,238,245); color: #4c3525; border: 1px solid #bea17a;
            border-radius: 10px; padding: 5px; font-size: 12px;
        }
        QListWidget#moveList::item { padding: 6px 3px; border: none; }
        QScrollBar:vertical { background: transparent; width: 6px; margin: 2px; }
        QScrollBar::handle:vertical { background: #b39976; border-radius: 2px; min-height: 20px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QComboBox, QLineEdit, QSpinBox {
            color: #4c3525; background: #fff7e9; border: 1px solid #bc996d;
            border-radius: 6px; padding: 7px;
        }
        QComboBox QAbstractItemView { color: #4c3525; background: #fff7e9; }
        QToolTip { color: #fff1d5; background: #5a3925; border: none; padding: 5px; }
    )").arg(gameSerifFontFamily(), mono));
}

#endif
