// SPDX-License-Identifier: GPL-3.0-or-later
#include "GameSettings.h"
#include "ChessVoice.h"
#include <QSettings>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QSlider>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>

GamePreferences GamePreferences::load()
{
    QSettings settings;
    GamePreferences result;
    result.sound = settings.value("game/sound", true).toBool();
    result.volume = qBound(0, settings.value("game/volume", 70).toInt(), 100);
    result.animations = settings.value("game/animations", true).toBool();
    result.animationPercent = qBound(50, settings.value("game/animationPercent", 100).toInt(), 150);
    result.traces = settings.value("game/traces", true).toBool();
    result.legalMoves = settings.value("game/legalMoves", true).toBool();
    result.confirmExit = settings.value("game/confirmExit", true).toBool();
    return result;
}

void GamePreferences::save() const
{
    QSettings settings;
    settings.setValue("game/sound", sound);
    settings.setValue("game/volume", volume);
    settings.setValue("game/animations", animations);
    settings.setValue("game/animationPercent", animationPercent);
    settings.setValue("game/traces", traces);
    settings.setValue("game/legalMoves", legalMoves);
    settings.setValue("game/confirmExit", confirmExit);
}

GameSettings::GameSettings(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("游戏设置"));
    setObjectName(QStringLiteral("gameSettingsDialog"));
    setMinimumWidth(360);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    auto *form = new QFormLayout;
    form->setSpacing(16);
    auto preferences = GamePreferences::load();
    auto *sound = new QCheckBox(QStringLiteral("启用音效"), this);
    sound->setChecked(preferences.sound);
    form->addRow(sound);
    auto *volume = new QSlider(Qt::Horizontal, this);
    volume->setRange(0, 100);
    volume->setValue(preferences.volume);
    volume->setAccessibleName(QStringLiteral("音效音量"));
    form->addRow(QStringLiteral("音量"), volume);
    auto *previewVoice = new ChessVoice(this);
    auto *preview = new QPushButton(QStringLiteral("试听“吃”和“将军”"), this);
    preview->setObjectName(QStringLiteral("previewSoundButton"));
    preview->setEnabled(preferences.sound);
    form->addRow(preview);
    connect(sound, &QCheckBox::toggled, preview, &QPushButton::setEnabled);
    connect(sound, &QCheckBox::toggled, this, [previewVoice](bool enabled) {
        if (!enabled)
            previewVoice->stop();
    });
    connect(volume, &QSlider::valueChanged, this, [previewVoice](int value) {
        previewVoice->setVolume(value / 100.0);
    });
    connect(preview, &QPushButton::clicked, this, [=] {
        previewVoice->stop();
        previewVoice->setVolume(volume->value() / 100.0);
        previewVoice->voiceEat();
        previewVoice->voiceGeneral();
    });
    connect(this, &QDialog::finished, previewVoice, &ChessVoice::stop);
    volume->setEnabled(preferences.sound);
    connect(sound, &QCheckBox::toggled, volume, &QSlider::setEnabled);
    auto *animations = new QCheckBox(QStringLiteral("启用走子动画"), this);
    animations->setChecked(preferences.animations);
    form->addRow(animations);
    auto *speed = new QComboBox(this);
    speed->addItem(QStringLiteral("轻缓"), 150);
    speed->addItem(QStringLiteral("标准"), 100);
    speed->addItem(QStringLiteral("快速"), 50);
    speed->setCurrentIndex(qMax(0, speed->findData(preferences.animationPercent)));
    speed->setEnabled(preferences.animations);
    connect(animations, &QCheckBox::toggled, speed, &QComboBox::setEnabled);
    form->addRow(QStringLiteral("动画速度"), speed);
    auto *traces = new QCheckBox(QStringLiteral("显示上一步走棋轨迹"), this);
    traces->setChecked(preferences.traces);
    form->addRow(traces);
    auto *legalMoves = new QCheckBox(QStringLiteral("选中棋子时显示合法落点"), this);
    legalMoves->setChecked(preferences.legalMoves);
    form->addRow(legalMoves);
    auto *confirmExit = new QCheckBox(QStringLiteral("退出进行中的对局前确认"), this);
    confirmExit->setChecked(preferences.confirmExit);
    form->addRow(confirmExit);
    layout->addLayout(form);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [=]() mutable {
        preferences.sound = sound->isChecked();
        preferences.volume = volume->value();
        preferences.animations = animations->isChecked();
        preferences.animationPercent = speed->currentData().toInt();
        preferences.traces = traces->isChecked();
        preferences.legalMoves = legalMoves->isChecked();
        preferences.confirmExit = confirmExit->isChecked();
        preferences.save();
        accept();
    });
}
