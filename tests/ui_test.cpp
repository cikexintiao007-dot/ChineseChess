// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/MachineGame.h"
#include "ui/ChooseMainWindow.h"
#include "ui/GameTheme.h"
#include "ui/GameResultDialog.h"
#include <QCheckBox>
#include <QListWidget>
#include <QSignalSpy>
#include <QMessageBox>
#include <QLabel>
#include <QStandardPaths>
#include <QDir>
#include <QTemporaryDir>
#include <QSettings>
#include <QScreen>
#include <QDialogButtonBox>
#include "game/PuzzleGame.h"
#include "game/SwapPuzzleGame.h"
#include <QApplication>
#include <QFontDatabase>
#include <QFile>
#include <QtEndian>
#include <QElapsedTimer>
#include <QTest>
#include <QPushButton>
#include <QComboBox>
#include <QThreadPool>
#include <iostream>
#include <random>
#include <vector>
#include <stdexcept>
#include "core/ChessRules.h"
#include "core/ChessSearch.h"
#include "game/ReplayGame.h"
#include "ui/GamePages.h"
#include "game/GameSession.h"
#include "game/PlayerProfiles.h"
#include <QTabWidget>
#include <QSlider>
#include <QTableWidget>
#include <QLineEdit>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void verifyAudioAssets()
{
    for (const auto &name : {"eatChess.wav", "generalSound.wav", "WinSound.wav", "moveChess.wav",
                             "selectChess.wav", "backChess.wav", "drawSound.wav"})
    {
        QFile file(QStringLiteral(":/sound/") + QString::fromLatin1(name));
        require(file.open(QIODevice::ReadOnly), "sound resource missing");
        const auto bytes = file.readAll();
        require(bytes.size() > 44 && bytes.left(4) == "RIFF" && bytes.mid(8, 4) == "WAVE", "invalid WAV resource");
        const auto *header = reinterpret_cast<const uchar *>(bytes.constData());
        require(qFromLittleEndian<quint16>(header + 20) == 1 && qFromLittleEndian<quint16>(header + 22) == 1
                    && qFromLittleEndian<quint32>(header + 24) == 44100
                    && qFromLittleEndian<quint16>(header + 34) == 16, "WAV must be mono 44.1kHz PCM16");
        require(bytes.size() < 120000, "sound prompt too long");
    }
    ChessVoice voice;
    voice.setVolume(0.0001);
    auto *capture = voice.findChild<QSoundEffect *>("eatChess.wav");
    auto *check = voice.findChild<QSoundEffect *>("generalSound.wav");
    require(capture && check, "voice cues not configured");
    QElapsedTimer loading;
    loading.start();
    while ((capture->status() == QSoundEffect::Loading || check->status() == QSoundEffect::Loading)
           && loading.elapsed() < 2500) QTest::qWait(5);
    if (capture->status() == QSoundEffect::Ready && check->status() == QSoundEffect::Ready)
    {
        QStringList started;
        QObject::connect(capture, &QSoundEffect::playingChanged, &voice, [&] {
            if (capture->isPlaying()) started.append("capture");
        });
        QObject::connect(check, &QSoundEffect::playingChanged, &voice, [&] {
            if (!check->isPlaying()) return;
            require(!capture->isPlaying(), "capture and check voices overlap");
            started.append("check");
        });
        voice.voiceEat();
        voice.voiceGeneral();
        QElapsedTimer playback;
        playback.start();
        while (started.size() < 2 && playback.elapsed() < 4000) QTest::qWait(5);
        require(started == QStringList({"capture", "check"}), "capture/check ordering failed");
        voice.stop();
        voice.voiceEat();
        voice.voiceGeneral();
        voice.setVolume(0);
        const int mutedCount = started.size();
        QTest::qWait(1000);
        require(started.size() == mutedCount && !capture->isPlaying() && !check->isPlaying(), "muted speech resumed");
        std::cout << "PASS audio PCM assets, ordered capture/check speech, stop and mute" << std::endl;
    }
    else
    {
        voice.stop();
        std::cout << "PASS audio PCM assets; playback check requires an available audio output" << std::endl;
    }
}

unsigned long long rulesChecksum(ChessBoard &game)
{
    std::mt19937 random(12345);
    unsigned long long hash = 1469598103934665603ULL;
    for (int ply = 0; ply < 60; ++ply)
    {
        struct Move
        {
            int id, target, row, col;
        };
        std::vector<Move> moves;
        const int start = game.m_bIsRed ? 16 : 0;
        for (int id = start; id < start + 16; ++id)
        {
            if (game.m_ChessPieces[id].m_bDead)
                continue;
            for (int row = 0; row < 10; ++row)
            {
                for (int col = 0; col < 9; ++col)
                {
                    const int target = game.getStoneId(row, col);
                    if (target != -1 && game.sameColor(id, target))
                        continue;
                    if (!game.canMove(id, target, row, col))
                        continue;
                    hash = (hash ^ (id * 10000 + (target + 1) * 100 + row * 9 + col)) * 1099511628211ULL;
                    if (target != 4 && target != 20)
                        moves.push_back({id, target, row, col});
                }
            }
        }
        if (moves.empty())
            break;
        const auto move = moves[random() % moves.size()];
        game.killStone(move.target);
        game.moveStone(move.id, move.row, move.col);
    }
    game.init();
    return hash;
}

qint64 humanMove(ChessBoard &game)
{
    game.show();
    QTest::qWait(30);
    QPointF origin;
    qreal side;
    require(game.boardTransform(origin, side), "board transform failed");
    auto point = [&](int row, int col) {
        return (origin + game.center(row, col) * (side / 960.0)).toPoint();
    };
    QElapsedTimer inputTimer;
    inputTimer.start();
    QTest::mouseClick(&game, Qt::LeftButton, Qt::NoModifier, point(6, 8));
    QTest::mouseClick(&game, Qt::LeftButton, Qt::NoModifier, point(5, 8));
    require(game.m_ChessSteps.size() == 1, "human move did not start through mouse entry");
    return inputTimer.elapsed();
}

void finishGame(ChessBoard &game, bool redWon)
{
    game.init();
    const int attacker = redWon ? 16 : 0;
    const int victim = redWon ? 4 : 20;
    for (int id = 0; id < 32; ++id)
        game.m_ChessPieces[id].m_bDead = id != 4 && id != 20 && id != attacker;
    game.m_ChessPieces[attacker].m_nRow = redWon ? 1 : 8;
    game.m_ChessPieces[attacker].m_nCol = 4;
    game.m_bIsRed = redWon;
    game.doMoveStone(attacker, victim, redWon ? 0 : 9, 4);
}

QDialog *waitForResult(ChessBoard &game)
{
    QElapsedTimer timer;
    timer.start();
    QDialog *dialog = nullptr;
    while (timer.elapsed() < 1500)
    {
        dialog = game.findChild<QDialog *>("gameResultDialog");
        if (dialog && dialog->isVisible())
            return dialog;
        QTest::qWait(5);
    }
    throw std::runtime_error("result dialog did not open");
}

void verifySwapChallenge(ChooseMainWindow &menu)
{
    auto *entry = menu.findChild<QPushButton *>("menuSwapPuzzleButton");
    require(entry && entry->isVisible(), "swap challenge is missing from the main menu");
    QTimer::singleShot(0, &menu, [&menu] {
        if (auto *page = menu.findChild<QDialog *>("puzzleSelectionPage"))
            if (auto *start = page->findChild<QPushButton *>("startPuzzleButton")) start->click();
    });
    entry->click();
    SwapPuzzleGame *game = nullptr;
    for (auto *window : QApplication::topLevelWidgets()) {
        if (auto *candidate = dynamic_cast<SwapPuzzleGame *>(window); candidate && candidate->isVisible())
            game = candidate;
    }
    require(game && !menu.isVisible(), "swap challenge did not open");
    game->resize(760, 500);
    QTest::qWait(30);
    auto *advance = game->findChild<QPushButton *>("swapAdvanceButton");
    require(advance && !advance->isVisible() && !game->isAiRed() && !game->m_bReverseView,
            "attack round began with incorrect controls or side");
    const auto startingPosition = ChessRules::positionKey(game->m_ChessPieces, game->m_bIsRed);
    const auto playBestHumanMove = [&] {
        require(QTest::qWaitFor([&] {
            return game->m_bIsOver || (game->m_bIsRed != game->isAiRed() && !game->isMoveAnimating());
        }, 5000), "swap challenge AI did not finish");
        if (game->m_bIsOver) return;
        ChessSearch::Position position;
        std::copy_n(game->m_ChessPieces, position.size(), position.begin());
        auto canceled = std::make_shared<std::atomic_bool>(false);
        ChessSearch search(position, game->m_bIsRed, game->m_bIsRed, true, canceled);
        const auto move = search.bestMove(5, 2000).move;
        require(move.moveId != -1, "swap challenge did not have a legal human move");
        QPointF origin;
        qreal side;
        require(game->boardTransform(origin, side), "swap challenge cannot locate board");
        const auto click = [&](int row, int col) {
            QTest::mouseClick(game, Qt::LeftButton, Qt::NoModifier,
                              (origin + game->center(row, col) * (side / 960.0)).toPoint());
        };
        const auto before = game->m_ChessSteps.size();
        click(move.rowFrom, move.colFrom);
        click(move.rowTo, move.colTo);
        require(game->m_ChessSteps.size() > before, "swap challenge rejected a legal human move");
    };
    for (int i = 0; i < 3; ++i) playBestHumanMove();
    require(QTest::qWaitFor([&] { return advance->isVisible(); }, 5000) &&
                game->outcome() == ChessBoard::Outcome::RedWon && game->m_ChessSteps.size() == 5,
            "winning attack was not recognized within three red moves");
    require(!game->findChild<QDialog *>("gameResultDialog"), "normal result dialog interrupted swap challenge");
    const auto attack = game->gameRecord();
    advance->click();
    require(game->isAiRed() && game->m_bReverseView && !advance->isVisible() &&
                ChessRules::positionKey(game->m_ChessPieces, game->m_bIsRed) == startingPosition,
            "defense did not restart the same position with colors swapped");
    playBestHumanMove();
    require(QTest::qWaitFor([&] { return advance->isVisible(); }, 5000) &&
                game->outcome() == ChessBoard::Outcome::Draw && game->m_ChessSteps.size() == 3 &&
                game->findChild<QLabel *>("headerState")->text() == QStringLiteral("目标达成"),
            "surviving two red moves was not recognized");
    bool compared = false;
    QTimer::singleShot(0, game, [&] {
        auto *page = game->findChild<QDialog *>("swapComparison");
        if (!page) return;
        QList<ReplayGame *> replays;
        for (auto *widget : page->findChildren<QWidget *>()) {
            if (auto *replay = dynamic_cast<ReplayGame *>(widget)) replays.append(replay);
        }
        if (replays.size() == 2) {
            auto *left = replays[0]->findChild<QSlider *>("replaySlider");
            auto *right = replays[1]->findChild<QSlider *>("replaySlider");
            QPointF origin;
            qreal side;
            if (left && right && replays[0]->isVisible() && replays[1]->isVisible() &&
                replays[0]->boardTransform(origin, side) && replays[1]->boardTransform(origin, side) &&
                left->maximum() == attack.moves.size() && right->maximum() == 3 &&
                !replays[0]->m_bReverseView && !replays[1]->m_bReverseView &&
                replays[0]->gameRecord().validate() && replays[1]->gameRecord().validate()) {
                left->setValue(0);
                compared = replays[0]->m_ChessSteps.isEmpty() && right->value() == right->maximum() &&
                           ChessRules::positionKey(replays[0]->m_ChessPieces, replays[0]->m_bIsRed) == startingPosition;
            }
        }
        page->reject();
    });
    advance->click();
    require(compared, "side-by-side replays did not load or seek independently");
    game->close();
    require(QTest::qWaitFor([&] { return menu.isVisible(); }, 1000), "swap challenge did not return to menu");
}

void verifyPlayerRanking(ChooseMainWindow &menu, QPushButton *localMode)
{
    auto *entry = menu.findChild<QPushButton *>("menuPlayersButton");
    require(entry && entry->isVisible(), "players menu entry is missing");
    bool created = false;
    QTimer::singleShot(0, &menu, [&] {
        auto *page = menu.findChild<QDialog *>("playerProfilesPage");
        if (!page) return;
        auto *add = page->findChild<QPushButton *>("playerAddButton");
        auto *table = page->findChild<QTableWidget *>("playerStandings");
        if (add && table && table->rowCount() == 0) {
            for (const QString &name : {QStringLiteral("松风"), QStringLiteral("竹影")}) {
                QTimer::singleShot(0, page, [page, name] {
                    auto *edit = page->findChild<QDialog *>("playerEditPage");
                    if (!edit) return;
                    edit->findChild<QLineEdit *>("playerNickname")->setText(name);
                    edit->findChild<QPushButton *>("playerSaveButton")->click();
                });
                add->click();
            }
            created = table->rowCount() == 2 && table->item(0, 0)->text() == QStringLiteral("—");
        }
        page->reject();
    });
    entry->click();
    QVector<PlayerProfile> profiles;
    require(created && PlayerProfiles::load(profiles) && profiles.size() == 2, "could not create two durable profiles");
    const auto red = profiles[0];
    const auto black = profiles[1];
    PlayerProfile duplicate{{}, red.name, red.avatar};
    require(!PlayerProfiles::save(duplicate), "duplicate nickname accepted");

    bool identityGuarded = false;
    QTimer::singleShot(0, &menu, [&] {
        auto *page = menu.findChild<QDialog *>("matchSetupPage");
        if (!page) return;
        auto *redChoice = page->findChild<QComboBox *>("matchRedProfile");
        auto *blackChoice = page->findChild<QComboBox *>("matchBlackProfile");
        auto *start = page->findChild<QPushButton *>("matchSetupStart");
        if (redChoice && blackChoice && start) {
            redChoice->setCurrentIndex(redChoice->findData(red.id));
            blackChoice->setCurrentIndex(blackChoice->findData(red.id));
            identityGuarded = !start->isEnabled();
            blackChoice->setCurrentIndex(blackChoice->findData(black.id));
            if (start->isEnabled()) start->click();
            else page->reject();
        } else {
            page->reject();
        }
    });
    localMode->click();
    const auto openBoard = [] {
        for (auto *window : QApplication::topLevelWidgets())
            if (auto *board = qobject_cast<ChessBoard *>(window); board && board->isVisible()) return board;
        return static_cast<ChessBoard *>(nullptr);
    };
    auto *board = openBoard();
    require(identityGuarded && board && board->gameRecord().redPlayerId == red.id
                && board->gameRecord().blackPlayerId == black.id, "player selections did not reach board");
    humanMove(*board);
    ChessRecord saved;
    const QString session = GameSession::latest(saved);
    require(!session.isEmpty() && saved.redPlayerId == red.id && saved.blackPlayerId == black.id,
            "unfinished rated match did not retain its identities");
    board->close();
    require(QTest::qWaitFor([&] { return menu.isVisible(); }, 1000), "could not return to menu after saving");
    menu.findChild<QPushButton *>("menuContinueButton")->click();
    board = openBoard();
    require(board && board->gameRecord().redPlayerId == red.id && board->gameRecord().blackPlayerId == black.id
                && board->m_ChessSteps.size() == 1 && !board->m_bIsRed,
            "continuing a rated match changed the identity or positions");
    QTimer::singleShot(0, board, [board] {
        if (auto *confirmation = board->findChild<QMessageBox *>())
            confirmation->button(QMessageBox::Yes)->click();
    });
    board->findChild<QPushButton *>("resignButton")->click();
    waitForResult(*board)->findChild<QPushButton *>("resultReview")->click();
    const auto finished = board->gameRecord();
    require(finished.outcome == 1 && GameSession::save(session, finished, nullptr)
                && !GameSession::save(session, saved, nullptr), "a completed match could be reopened or not resaved");
    auto conflicting = finished;
    conflicting.outcome = 2;
    require(!GameSession::save(session, conflicting, nullptr), "conflicting result replaced the first settlement");
    board->close();
    require(QTest::qWaitFor([&] { return menu.isVisible(); }, 1000), "menu did not return after rated match");

    auto guest = finished;
    guest.blackPlayerId.clear();
    auto empty = finished;
    empty.moves.clear();
    auto computer = guest;
    computer.computer = true;
    computer.redPlayerId.clear();
    require(GameSession::save(GameSession::newId(), guest, nullptr)
        && GameSession::save(GameSession::newId(), empty, nullptr)
        && GameSession::save(GameSession::newId(), computer, nullptr), "unable to save unranked fixtures");
    auto draw = finished;
    draw.outcome = 3;
    draw.reason = QStringLiteral("双方同意和棋");
    require(GameSession::save(GameSession::newId(), draw, nullptr), "draw fixture could not be saved");
    auto renamed = red;
    renamed.name = QStringLiteral("松风棋手");
    renamed.avatar = QStringLiteral("马");
    require(PlayerProfiles::save(renamed) && PlayerProfiles::load(profiles) && renamed.id == red.id,
            "updating a profile changed its identity");
    const auto matches = PlayerProfiles::ratedMatches(profiles);
    const auto standings = PlayerProfiles::standings(profiles, matches);
    require(matches.size() == 2 && standings.size() == 2 && standings[0].profile.name == renamed.name
                && standings[0].points() == 4 && standings[0].wins == 1 && standings[0].draws == 1
                && standings[1].points() == 1 && standings[1].losses == 1,
            "ranking lost history, double-counted a game, or included an ineligible match");
    bool compared = false;
    QTimer::singleShot(0, &menu, [&] {
        auto *page = menu.findChild<QDialog *>("playerProfilesPage");
        if (!page) return;
        auto *table = page->findChild<QTableWidget *>("playerStandings");
        auto *opponents = page->findChild<QPushButton *>("playerHeadToHeadButton");
        if (table && table->rowCount() == 2 && table->item(0, 2)->text() == QStringLiteral("4") && opponents) {
            opponents->click();
            auto *history = page->findChild<QListWidget *>("headToHeadMatches");
            auto *replay = page->findChild<QPushButton *>("headToHeadReplay");
            if (history && history->count() == 2 && replay && replay->isEnabled()) {
                compared = true;
                replay->click();
                if (!page->isVisible()) return;
            }
        }
        page->reject();
    });
    entry->click();
    auto *replay = dynamic_cast<ReplayGame *>(openBoard());
    require(compared && replay && replay->gameRecord().moves.size() == 1 &&
                replay->gameRecord().redPlayerId == red.id, "could not replay a head-to-head game");
    replay->close();
    require(QTest::qWaitFor([&] { return menu.isVisible(); }, 1000), "could not return to lobby from ranked replay");
}

void verifyRepetitions()
{
    ChessBoard game;
    require(game.repetitionCount() == 1, "initial repetition count wrong");
    for (int cycle = 0; cycle < 2; ++cycle) {
        game.doMoveStone(26, -1, 7, 2);
        game.doMoveStone(9, -1, 2, 2);
        game.doMoveStone(26, -1, 7, 1);
        game.doMoveStone(9, -1, 2, 1);
    }
    require(game.repetitionCount() == 3 && !game.m_bIsOver,
            "threefold repetition not detected or forced a result");
    require(game.findChild<QLabel *>("labelHint")->text().contains(QStringLiteral("重复 3 次")),
            "repetition hint missing");
    const auto record = game.gameRecord();
    ChessBoard restored;
    QString error;
    require(restored.resumeRecord(record, &error) && restored.repetitionCount() == 3,
            "repetition history lost on load");
    restored.back();
    require(restored.repetitionCount() == 2, "undo retained stale repetition count");
    restored.init();
    require(restored.repetitionCount() == 1, "restart retained repetition history");
    ReplayGame replay(record);
    require(replay.repetitionCount() == 3, "replay repetition count wrong");
    replay.findChild<QPushButton *>("replayFirstButton")->click();
    require(replay.repetitionCount() == 1, "replay rewind retained future repetitions");
    game.doMoveStone(31, -1, 5, 0);
    game.doMoveStone(11, -1, 4, 0);
    game.doMoveStone(31, 11, 4, 0);
    require(game.repetitionCount() == 1, "capture retained earlier repetition count");
    std::cout << "PASS repetition warning, undo/restart, record load, replay and capture reset" << std::endl;
}

void verifyRecords()
{
    QTemporaryDir directory;
    ChessBoard source;
    source.doMoveStone(31, -1, 5, 0);
    source.doMoveStone(11, -1, 4, 0);
    source.doMoveStone(31, 11, 4, 0);
    const auto record = source.gameRecord();
    QString error;
    require(record.validate(&error), "captured-piece record invalid");
    require(record.initial[11].m_nRow == 3 && !record.initial[11].m_bDead,
            "record did not reconstruct initial captured piece");
    const QString path = directory.filePath(QStringLiteral("中文棋谱.cchess"));
    require(record.save(path, &error), "record save failed");
    ChessRecord loaded;
    require(ChessRecord::load(path, loaded, &error), "record load failed");
    ChessBoard restored;
    require(restored.resumeRecord(loaded, &error), "record resume failed");
    require(restored.m_ChessSteps.size() == 3 && !restored.m_bIsRed && restored.m_ChessPieces[11].m_bDead,
            "record turn/capture/history lost");
    restored.back();
    require(!restored.m_ChessPieces[11].m_bDead && restored.m_ChessPieces[31].m_nRow == 5,
            "undo after record resume failed");
    restored.init();
    require(restored.m_ChessSteps.isEmpty() && restored.m_ChessPieces[31].m_nRow == 6,
            "restart after record resume failed");
    ReplayGame replay(loaded);
    replay.show();
    QTest::qWait(20);
    auto *slider = replay.findChild<QSlider *>("replaySlider");
    require(slider && slider->value() == 3, "replay did not open at final position");
    replay.findChild<QPushButton *>("replayFirstButton")->click();
    require(replay.m_ChessSteps.isEmpty() && !replay.m_ChessPieces[11].m_bDead,
            "replay initial position wrong");
    replay.findChild<QPushButton *>("replayNextButton")->click();
    require(replay.m_ChessSteps.size() == 1 && replay.m_ChessPieces[31].m_nRow == 5,
            "replay next move failed");
    slider->setValue(3);
    require(replay.m_ChessPieces[11].m_bDead, "replay seek lost capture");
    slider->setValue(1);
    replay.findChild<QPushButton *>("replayContinueButton")->click();
    ChessBoard *branch = nullptr;
    for (auto *widget : QApplication::topLevelWidgets()) {
        auto *candidate = qobject_cast<ChessBoard *>(widget);
        if (candidate && candidate->windowTitle() == QStringLiteral("棋谱续局")) branch = candidate;
    }
    require(branch && branch->m_ChessSteps.size() == 1 && !branch->m_bIsRed
                && source.m_ChessSteps.size() == 3, "replay branch changed original game");
    branch->close();
    QTest::qWait(20);
    require(replay.isVisible(), "closing resumed game did not return to replay");
    slider->setValue(3);
    replay.resize(760, 500);
    QTest::qWait(20);
    for (const auto &name : {"replayFirstButton", "replayNextButton", "replayContinueButton"}) {
        auto *button = replay.findChild<QPushButton *>(QString::fromLatin1(name));
        require(button->isVisible() && replay.rect().contains(QRect(button->mapTo(&replay, QPoint()), button->size())),
                "compact replay control clips");
    }
    const QString output = QDir::tempPath() + "/ChineseChess-test-artifacts/";
    QDir().mkpath(output);
    replay.grab().save(output + "replay-compact.png");
    replay.close();
    auto finished = record;
    finished.outcome = 1;
    finished.reason = QStringLiteral("黑方认输");
    ReplayGame completed(finished);
    require(completed.outcome() == ChessBoard::Outcome::RedWon
                && !completed.findChild<QPushButton *>("replayContinueButton")->isEnabled(),
            "finished record result lost or terminal resume enabled");
    completed.findChild<QPushButton *>("replayPreviousButton")->click();
    require(completed.findChild<QPushButton *>("replayContinueButton")->isEnabled(),
            "finished record cannot branch before final position");
    auto invalid = record;
    invalid.moves[0].row = 4;
    require(!invalid.validate(&error) && !restored.resumeRecord(invalid, &error)
                && restored.m_ChessSteps.isEmpty(), "illegal record mutated live board");
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "cannot read test record");
    auto object = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    object["seconds"] = 1.5;
    require(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "cannot write malformed record");
    file.write(QJsonDocument(object).toJson());
    file.close();
    require(!ChessRecord::load(path, loaded, &error) && loaded.moves.size() == 3,
            "malformed record partially replaced output");
    require(record.save(path, &error) && ChessRecord::load(path, loaded, &error),
            "atomic record replacement failed");
    PuzzleGame puzzle(getBuiltinPuzzles().front(), AIDifficulty::Easy);
    const auto puzzleRecord = puzzle.gameRecord();
    require(puzzleRecord.validate(&error), "puzzle record invalid");
    ChessBoard puzzleCopy;
    require(puzzleCopy.resumeRecord(puzzleRecord, &error), "puzzle resume failed");
    for (int id = 0; id < 32; ++id)
        require(puzzleCopy.m_ChessPieces[id].m_bDead == puzzle.m_ChessPieces[id].m_bDead
                    && puzzleCopy.m_ChessPieces[id].m_nRow == puzzle.m_ChessPieces[id].m_nRow
                    && puzzleCopy.m_ChessPieces[id].m_nCol == puzzle.m_ChessPieces[id].m_nCol,
                "puzzle initial position changed");
    MachineGame machine(AIDifficulty::Easy);
    machine.doMoveStone(31, -1, 5, 0);
    const auto aiRecord = machine.gameRecord();
    MachineGame resumed;
    require(resumed.resumeRecord(aiRecord, &error) && resumed.difficulty() == AIDifficulty::Easy,
            "AI record difficulty lost");
    QElapsedTimer wait;
    wait.start();
    while (resumed.m_ChessSteps.size() < 2 && wait.elapsed() < 5000) QTest::qWait(10);
    require(resumed.m_ChessSteps.size() == 2 && resumed.m_bIsRed, "resumed AI did not make its move");
    resumed.close();
    std::cout << "PASS records save/load, validation, capture/undo, puzzle, replay and AI resume" << std::endl;
}

void verifyTimedMatch(ChooseMainWindow &menu, QPushButton *localMode, const QString &output)
{
    QTimer::singleShot(40, &menu, [&] {
        auto *page = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!page) return;
        auto *control = page->findChild<QComboBox *>("matchTimeControl");
        auto *start = page->findChild<QPushButton *>("matchSetupStart");
        if (!control || !start) {
            page->reject();
            return;
        }
        control->setCurrentIndex(1);
        page->grab().save(output + "match-setup.png");
        start->click();
    });
    localMode->click();
    const auto visibleBoard = []() -> ChessBoard * {
        for (auto *widget : QApplication::topLevelWidgets())
            if (auto *board = qobject_cast<ChessBoard *>(widget); board && board->isVisible()) return board;
        return nullptr;
    };
    auto *timed = visibleBoard();
    require(timed && timed->gameRecord().clockLimitMs == 300000, "timed match setup failed");
    QTest::qWait(140);
    timed->doMoveStone(31, -1, 5, 0);
    QTest::qWait(320);
    const auto live = timed->gameRecord();
    require(!timed->m_bIsRed && live.redRemainingMs < 300000 && live.blackRemainingMs < 300000
            && !timed->findChild<QPushButton *>("pushButton_back")->isEnabled(), "clock did not switch on legal move");
    timed->resize(760, 500);
    QTest::qWait(20);
    const auto *redClock = timed->findChild<QLabel *>("redMatchClock");
    const auto *blackClock = timed->findChild<QLabel *>("blackMatchClock");
    require(redClock->isVisible() && blackClock->isVisible() && blackClock->property("clockActive").toBool(),
            "clock display or active indicator missing");
    timed->grab().save(output + "timed-match.png");
    timed->close();
    QTest::qWait(40);
    auto *continueButton = menu.findChild<QPushButton *>("menuContinueButton");
    ChessRecord checkpoint;
    const auto timedId = GameSession::latest(checkpoint);
    require(menu.isVisible() && continueButton->isEnabled() && !timedId.isEmpty()
                && checkpoint.moves.size() == 1 && checkpoint.blackRemainingMs <= live.blackRemainingMs,
            "closing match failed to checkpoint clock and move history");
    continueButton->click();
    timed = visibleBoard();
    require(timed && !timed->m_bIsRed && timed->m_ChessSteps.size() == 1
                && timed->gameRecord().redRemainingMs == checkpoint.redRemainingMs,
            "continue did not restore the saved turn and both clocks");
    timed->close();
    QTest::qWait(40);
    checkpoint.blackRemainingMs = 120;
    ChessBoard expiring;
    require(expiring.resumeRecord(checkpoint), "short remaining clock could not be restored");
    expiring.enableSessionSaving(timedId);
    QElapsedTimer waitForClock;
    waitForClock.start();
    while (!expiring.m_bIsOver && waitForClock.elapsed() < 2500) QTest::qWait(20);
    require(expiring.outcome() == ChessBoard::Outcome::RedWon
                && expiring.resultReason() == QStringLiteral("黑方超时"), "clock did not settle the game");
    const QString ending = timedId + QStringLiteral(".cchess");
    const auto completed = GameSession::files(true);
    const auto unfinished = GameSession::files(false);
    const auto byId = [&ending](const QString &path) { return path.endsWith(ending); };
    const auto savedResult = std::find_if(completed.cbegin(), completed.cend(), byId);
    ChessRecord archived;
    require(savedResult != completed.cend() && std::none_of(unfinished.cbegin(), unfinished.cend(), byId)
                && ChessRecord::load(*savedResult, archived) && archived.outcome == 1
                && archived.blackRemainingMs == 0, "timeout archive or resume cleanup failed");
    expiring.close();
    std::cout << "PASS timed setup, turn switch, automatic save, resume, timeout and archive" << std::endl;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
#ifdef Q_OS_WIN
    // The offscreen platform does not enumerate the Windows font collection.
    const QString fonts = qEnvironmentVariable("WINDIR") + "/Fonts/";
    for (const QString &file :
         {QStringLiteral("msyh.ttc"), QStringLiteral("msyhbd.ttc"), QStringLiteral("simkai.ttf")})
        QFontDatabase::addApplicationFont(fonts + file);
#endif
    QTemporaryDir settingsDirectory;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
    app.setOrganizationName("ChineseChessTests");
    app.setApplicationName(QStringLiteral("ui-") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    auto preferences = GamePreferences::load();
    preferences.confirmExit = false;
    preferences.sound = false;
    preferences.save();
    applyGameTheme(app);
    app.setQuitOnLastWindowClosed(false);
    try
    {
        verifyRecords();
        verifyRepetitions();
        verifyAudioAssets();
        MachineGame game(argc > 1 ? AIDifficulty::Hard : AIDifficulty::Medium);
        std::cout << "rules_checksum=" << rulesChecksum(game) << std::endl;
        require(!game.findChild<QPushButton *>("pushButton_about"), "author button still exists");
        require(QFontMetrics(app.font()).inFont(QChar(0x68cb)), "Chinese font fallback unavailable");
        GameSettings settingsDialog;
        settingsDialog.show();
        QTest::qWait(20);
        for (auto *checkbox : settingsDialog.findChildren<QCheckBox *>())
        {
            if (checkbox->text().contains(QStringLiteral("合法落点")))
                checkbox->setChecked(false);
        }
        settingsDialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        require(!GamePreferences::load().legalMoves, "settings dialog did not persist changes");
        preferences.legalMoves = true;
        preferences.save();
        require(game.metaObject()->indexOfSlot("on_pushButton_about_clicked()") == -1,
                "author slot still exists");
        const QPalette originalPalette = app.palette();
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor("#202020"));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Button, QColor("#303030"));
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        app.setPalette(darkPalette);
        ChooseMainWindow menu;
        require(menu.windowTitle() == QStringLiteral("中国象棋"), "main menu title contains version");
        const QSize availableMenuSize = menu.screen()->availableGeometry().size() - QSize(32, 56);
        require(menu.size() == QSize(620, 680).boundedTo(availableMenuSize).expandedTo(menu.minimumSize()),
                "main menu startup size does not follow available screen space");
        menu.show();
        QTest::qWait(50);
        auto modeButtons = menu.findChildren<QPushButton *>(QString(), Qt::FindDirectChildrenOnly);
        modeButtons.removeAll(menu.findChild<QPushButton *>("menuSettingsButton"));
        modeButtons.removeAll(menu.findChild<QPushButton *>("menuOpenRecordButton"));
        modeButtons.removeAll(menu.findChild<QPushButton *>("menuGuideButton"));
        modeButtons.removeAll(menu.findChild<QPushButton *>("menuSetupButton"));
        modeButtons.removeAll(menu.findChild<QPushButton *>("menuContinueButton"));
        modeButtons.removeAll(menu.findChild<QPushButton *>("menuSwapPuzzleButton"));
        modeButtons.removeAll(menu.findChild<QPushButton *>("menuPlayersButton"));
        require(modeButtons.size() == 4, "menu must have four modes");
        for (auto *button : modeButtons)
        {
            require(button->palette().color(QPalette::ButtonText) == QColor("#4c2f1e"),
                    "dark palette overrides menu button text");
            require(button->height() >= 60, "mode button too short");
            require(button->fontMetrics().horizontalAdvance(button->text()) + 44 <= button->width(),
                    "mode text clips");
        }
        const QString output = QDir::tempPath() + "/ChineseChess-test-artifacts/";
        QDir().mkpath(output);
        menu.grab().save(output + "menu-dark.png");
        modeButtons[0]->setFocus();
        QTest::keyClick(modeButtons[0], Qt::Key_Tab);
        require(modeButtons[1]->hasFocus(), "menu keyboard tab order failed");
        modeButtons[1]->setDown(true);
        menu.grab().save(output + "menu-pressed.png");
        modeButtons[1]->setDown(false);
        menu.resize(menu.minimumSize());
        QTest::qWait(30);
        for (auto *button : modeButtons)
            require(menu.rect().contains(button->geometry()), "button outside minimum menu size");
        menu.grab().save(output + "menu-small.png");
        bool setupReady = false;
        QTimer::singleShot(40, &menu, [&] {
            auto *page = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!page) return;
            auto *board = page->findChild<ChessBoard *>("setupBoard");
            auto *start = page->findChild<QPushButton *>("setupStart");
            auto *turn = page->findChild<QComboBox *>("setupTurn");
            QPointF origin;
            qreal side = 0;
            if (!board || !start || !turn || !board->boardTransform(origin, side)) {
                page->reject();
                return;
            }
            const auto square = [&](int row, int col) {
                return (origin + board->center(row, col) * (side / 960)).toPoint();
            };
            auto *erase = page->findChild<QPushButton *>("setupErase");
            if (!erase) {
                page->reject();
                return;
            }
            erase->click();
            QTest::mouseClick(board, Qt::LeftButton, Qt::NoModifier, square(9, 4));
            const bool missingKingRejected = !start->isEnabled();
            page->findChild<QPushButton *>("setupPiece7")->click();
            QTest::mouseClick(board, Qt::LeftButton, Qt::NoModifier, square(5, 4));
            const bool outsidePalaceRejected = !start->isEnabled();
            erase->click();
            QTest::mouseClick(board, Qt::LeftButton, Qt::NoModifier, square(5, 4));
            page->findChild<QPushButton *>("setupPiece7")->click();
            QTest::mouseClick(board, Qt::LeftButton, Qt::NoModifier, square(9, 4));
            erase->click();
            QTest::mouseClick(board, Qt::LeftButton, Qt::NoModifier, square(6, 0));
            turn->setCurrentIndex(1);
            setupReady = missingKingRejected && outsidePalaceRejected && start->isEnabled();
            page->grab().save(output + "position-editor.png");
            if (setupReady) start->click();
            else page->reject();
        });
        menu.findChild<QPushButton *>("menuSetupButton")->click();
        ChessBoard *customGame = nullptr;
        for (auto *widget : QApplication::topLevelWidgets()) {
            auto *board = qobject_cast<ChessBoard *>(widget);
            if (board && board->isVisible() && board->windowTitle() == QStringLiteral("自定义对局")) customGame = board;
        }
        require(setupReady && customGame && !customGame->m_bIsRed && customGame->m_ChessPieces[31].m_bDead,
                "position editor validation or custom game startup failed");
        auto customRecord = customGame->gameRecord();
        const QString setupPath = settingsDirectory.filePath(QStringLiteral("自定义残局.cchess"));
        ChessRecord loadedSetup;
        require(customRecord.save(setupPath) && ChessRecord::load(setupPath, loadedSetup)
                    && !loadedSetup.redFirst && loadedSetup.initial[31].m_bDead,
                "custom setup record round trip failed");
        customGame->init();
        require(!customGame->m_bIsRed && customGame->m_ChessPieces[31].m_bDead, "custom restart lost the setup");
        customGame->close();
        QTest::qWait(30);
        require(menu.isVisible(), "closing custom game did not return to menu");
        std::cout << "PASS position editor mouse controls, setup validation, launch, restart and file round trip" << std::endl;
        bool guideReady = false;
        QTimer::singleShot(40, &menu, [&] {
            auto *page = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!page) return;
            auto *tabs = page->findChild<QTabWidget *>();
            guideReady = page->objectName() == "chessGuidePage" && tabs && tabs->count() == 3;
            if (guideReady) {
                tabs->setCurrentIndex(1);
                page->grab().save(output + "chess-guide.png");
            }
            page->reject();
        });
        menu.findChild<QPushButton *>("menuGuideButton")->click();
        require(guideReady && menu.isVisible(), "guide opening or navigation failed");

        const QString libraryPath = settingsDirectory.filePath(QStringLiteral("测试棋谱.cchess"));
        ChessRecord libraryRecord;
        libraryRecord.mode = QStringLiteral("本地双人");
        libraryRecord.moves.append({31, -1, 5, 0});
        require(libraryRecord.save(libraryPath), "library test record save failed");
        rememberChessRecord(libraryPath);
        rememberChessRecord(libraryPath);
        bool libraryReady = false;
        QTimer::singleShot(40, &menu, [&] {
            auto *page = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!page) return;
            auto *list = page->findChild<QListWidget *>("recordLibraryList");
            auto *summary = page->findChild<QLabel *>("recordLibrarySummary");
            auto *open = page->findChild<QPushButton *>("recordLibraryOpen");
            libraryReady = list && list->count() == 1 && summary && summary->text().contains(QStringLiteral("1 步"))
                && open && open->isEnabled();
            if (libraryReady) {
                page->grab().save(output + "record-library.png");
                open->click();
            } else {
                page->reject();
            }
        });
        menu.findChild<QPushButton *>("menuOpenRecordButton")->click();
        ChessBoard *libraryReplay = nullptr;
        for (auto *widget : QApplication::topLevelWidgets()) {
            auto *board = qobject_cast<ChessBoard *>(widget);
            if (board && board->isVisible() && dynamic_cast<ReplayGame *>(board)) libraryReplay = board;
        }
        require(libraryReady && libraryReplay && libraryReplay->m_ChessSteps.size() == 1,
                "recent record did not open the correct replay");
        libraryReplay->close();
        QTest::qWait(30);
        require(menu.isVisible(), "closing library replay did not return to menu");
        std::cout << "PASS guide tabs and recent library to replay navigation" << std::endl;
        verifyTimedMatch(menu, modeButtons[0], output);
        verifySwapChallenge(menu);
        verifyPlayerRanking(menu, modeButtons[0]);
        QTimer chooseDefaults;
        QObject::connect(&chooseDefaults, &QTimer::timeout, [&output]() {
            if (auto *page = QApplication::activeModalWidget()) {
                if (auto *start = page->findChild<QPushButton *>("matchSetupStart")) {
                    start->click();
                    return;
                }
                if (auto *start = page->findChild<QPushButton *>("startPuzzleButton")) {
                    page->grab().save(output + "puzzle-selection.png");
                    start->click();
                }
            }
        });
        chooseDefaults.start(10);
        for (int mode = 0; mode < 3; ++mode)
        {
            menu.show();
            modeButtons[mode]->click();
            QTest::qWait(30);
            ChessBoard *opened = nullptr;
            for (QWidget *widget : QApplication::topLevelWidgets())
            {
                auto *candidate = qobject_cast<ChessBoard *>(widget);
                if (candidate && candidate->isVisible())
                    opened = candidate;
            }
            require(opened && !menu.isVisible(), "menu mode did not open game");
            const QStringList expectedTitles = {QStringLiteral("玩家自己对战"),
                                                QStringLiteral("玩家和AI对战 [中等]"),
                                                QStringLiteral("残局挑战 - 双车错 [中等]")};
            require(opened->windowTitle() == expectedTitles[mode], "game title contains version");
            require(!opened->findChild<QWidget *>("networkGroup"), "network panel remains");
            require(mode != 1 || dynamic_cast<MachineGame *>(opened), "AI menu wiring wrong");
            require(mode != 2 || dynamic_cast<PuzzleGame *>(opened), "puzzle menu wiring wrong");
            opened->findChild<QPushButton *>("pushButton_toMenu")->click();
            QTest::qWait(30);
            require(menu.isVisible(), "return to menu failed");
        }
        chooseDefaults.stop();
        app.setPalette(originalPalette);
        menu.close();
        std::cout << "PASS three local modes, mode navigation, return to menu, no network controls, dark "
                     "palette, keyboard focus"
                  << std::endl;
        ChessBoard local;
        local.resize(1100, 760);
        local.show();
        QTest::qWait(80);
        local.grab().save(output + "board-initial.png");
        QElapsedTimer renderTimer;
        renderTimer.start();
        for (int frame = 0; frame < 40; ++frame)
            local.grab();
        std::cout << "cached_render_average_ms=" << renderTimer.elapsed() / 40.0 << std::endl;

        auto *history = local.findChild<QListWidget *>("moveList");
        require(history && history->count() == 1 &&
                    !local.findChild<QPushButton *>("pushButton_back")->isEnabled(),
                "empty history/control state wrong");
        auto square = [&](int row, int col) {
            QPointF origin;
            qreal side;
            require(local.boardTransform(origin, side), "local board transform failed");
            return (origin + local.center(row, col) * (side / 960.0)).toPoint();
        };
        auto clickSquare = [&](int row, int col) {
            QTest::mouseClick(&local, Qt::LeftButton, Qt::NoModifier, square(row, col));
        };
        clickSquare(6, 8);
        local.grab().save(output + "board-selected.png");
        QSignalSpy frames(local.findChild<QVariantAnimation *>(), &QVariantAnimation::valueChanged);
        clickSquare(5, 8);
        require(local.isMoveAnimating() && local.m_ChessPieces[27].m_nRow == 5,
                "move animation did not start");
        QTest::qWait(40);
        clickSquare(3, 8);
        require(local.m_nSelectID == -1, "input accepted during animation");
        QTest::qWait(210);
        std::cout << "first_animation_frames=" << frames.count() << " running=" << local.isMoveAnimating()
                  << std::endl;
        require(!local.isMoveAnimating() && frames.count() >= 5, "animation failed to settle smoothly");
        require(history->count() == 1 && history->item(0)->text().contains(QStringLiteral("红")),
                "history missing human move");
        clickSquare(3, 8);
        clickSquare(4, 8);
        QElapsedTimer animationWait;
        animationWait.start();
        while (local.isMoveAnimating() && animationWait.elapsed() < 1500)
            QTest::qWait(10);
        require(!local.isMoveAnimating(), "second move animation did not settle");
        clickSquare(5, 8);
        clickSquare(4, 8);
        require(local.isMoveAnimating() && local.m_ChessPieces[15].m_bDead,
                "capture animation did not start");
        QTest::qWait(60);
        QTest::qWait(200);
        require(history->count() == 3, "capture not recorded");
        require(local.findChild<QLabel *>("capturedByRed")->text().contains(QStringLiteral("卒×1")),
                "captured summary not updated");
        local.grab().save(output + "board-played.png");
        local.back();
        require(history->count() == 2 && !local.m_ChessPieces[15].m_bDead &&
                    local.m_ChessPieces[27].m_nRow == 5,
                "capture undo/history failed");
        local.findChild<QPushButton *>("pushButton_restart")->click();
        humanMove(local);
        local.findChild<QPushButton *>("pushButton_back")->click();
        require(!local.isMoveAnimating() && local.m_ChessSteps.isEmpty(), "undo did not cancel animation");
        humanMove(local);
        local.findChild<QCheckBox *>("checkAnimations")->setChecked(false);
        require(!local.isMoveAnimating(), "animation toggle did not settle piece");
        local.findChild<QPushButton *>("pushButton_restart")->click();
        local.resize(760, 500);
        QTest::qWait(40);
        local.resize(760, 500);
        QTest::qWait(40);
        std::cout << "compact_size=" << local.width() << "x" << local.height() << std::endl;
        for (const QString &name :
             {QStringLiteral("statusCard"), QStringLiteral("moveList"), QStringLiteral("pushButton_toMenu")})
        {
            auto *widget = local.findChild<QWidget *>(name);
            const QRect rect(widget->mapTo(&local, QPoint()), widget->size());
            require(local.rect().contains(rect), "sidebar clips in compact window");
        }
        clickSquare(6, 8);
        clickSquare(5, 8);
        require(local.m_ChessSteps.size() == 1 && !local.isMoveAnimating(),
                "compact coordinates or disabled animation failed");
        local.grab().save(output + "board-compact.png");
        local.close();
        auto savedPreferences = GamePreferences::load();
        require(!savedPreferences.animations, "animation preference not saved");
        savedPreferences.animations = true;
        savedPreferences.save();
        savedPreferences.confirmExit = true;
        savedPreferences.save();
        ChessBoard exitConfirmation;
        humanMove(exitConfirmation);
        QTimer::singleShot(0, [] {
            auto *dialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            require(dialog, "exit confirmation missing");
            dialog->done(QMessageBox::No);
        });
        require(!exitConfirmation.close() && exitConfirmation.isVisible(), "canceling exit closed the game");
        QTimer::singleShot(0, [] {
            auto *dialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            require(dialog, "second exit confirmation missing");
            dialog->done(QMessageBox::Yes);
        });
        require(exitConfirmation.close(), "confirmed exit failed");
        savedPreferences.confirmExit = false;
        savedPreferences.save();
        std::cout
            << "PASS animation frames=" << frames.count()
            << " capture fade, capture undo, history, animation interruption, compact layout and coordinates"
            << std::endl;
        MachineGame quick(AIDifficulty::Easy);
        humanMove(quick);
        QTest::qWait(40);
        require(quick.m_ChessSteps.size() == 1 && quick.isMoveAnimating(),
                "fast AI interrupted human animation");
        quick.findChild<QPushButton *>("pushButton_restart")->click();
        QTest::qWait(300);
        require(quick.m_ChessSteps.isEmpty() && !quick.isMoveAnimating(),
                "deferred AI result survived restart");
        humanMove(quick);
        QElapsedTimer quickWait;
        quickWait.start();
        while ((quick.m_ChessSteps.size() != 2 || quick.isMoveAnimating()) && quickWait.elapsed() < 1200)
            QTest::qWait(5);
        require(quick.m_ChessSteps.size() == 2 && !quick.isMoveAnimating(),
                "fast AI animation sequence failed");
        require(quick.findChild<QListWidget *>("moveList")->count() == 2, "AI history row missing");
        quick.close();
        auto *quickClosing = new MachineGame(AIDifficulty::Easy);
        humanMove(*quickClosing);
        QTest::qWait(40);
        quickClosing->close();
        delete quickClosing;
        QTest::qWait(250);
        ChessBoard winBoard;
        winBoard.show();
        QTest::qWait(30);
        for (int id = 0; id < 32; ++id)
            winBoard.m_ChessPieces[id].m_bDead = id != 4 && id != 20 && id != 16;
        winBoard.m_ChessPieces[16].m_nRow = 1;
        winBoard.m_ChessPieces[16].m_nCol = 4;
        winBoard.doMoveStone(16, 4, 0, 4);
        require(winBoard.m_bIsOver && winBoard.isMoveAnimating(), "winning move animation state failed");
        winBoard.findChild<QPushButton *>("pushButton_restart")->click();
        QTest::qWait(300);
        require(!winBoard.m_bIsOver && !winBoard.findChild<QDialog *>("gameResultDialog"),
                "stale result dialog survived restart");
        winBoard.close();
        std::cout
            << "PASS fast-AI animation sequencing, pending-result restart/close, winning-animation restart"
            << std::endl;
        ChessBoard resultBoard;
        resultBoard.show();
        QTest::qWait(30);
        finishGame(resultBoard, true);
        require(resultBoard.isMoveAnimating() && !resultBoard.findChild<QDialog *>("gameResultDialog"),
                "result appeared before winning animation");
        auto *resultDialog = waitForResult(resultBoard);
        require(!resultBoard.isMoveAnimating(), "result interrupts animation");
        require(resultDialog->findChild<QLabel *>("resultWinner")->text() == QStringLiteral("红方获胜"),
                "red winner wrong");
        require(resultDialog->findChild<QLabel *>("resultSteps")->text() == QStringLiteral("1 步"),
                "result move count wrong");
        require(resultDialog->findChild<QLabel *>("resultTime")->text() ==
                    resultBoard.findChild<QLabel *>("lcdNumber")->text(),
                "result timer wrong");
        QTest::keyClick(resultDialog, Qt::Key_Return);
        QTest::qWait(30);
        require(resultBoard.m_bIsOver && resultBoard.m_ChessSteps.size() == 1 &&
                    !resultBoard.findChild<QDialog *>("gameResultDialog"),
                "Enter did not preserve final board");
        finishGame(resultBoard, false);
        resultDialog = waitForResult(resultBoard);
        require(resultDialog->findChild<QLabel *>("resultWinner")->text() == QStringLiteral("黑方获胜"),
                "black winner wrong");
        resultDialog->findChild<QPushButton *>("resultAgain")->click();
        QTest::qWait(50);
        require(!resultBoard.m_bIsOver && resultBoard.m_ChessSteps.isEmpty() &&
                    !resultBoard.m_ChessPieces[4].m_bDead && !resultBoard.m_ChessPieces[20].m_bDead,
                "result replay failed");
        finishGame(resultBoard, true);
        resultBoard.init();
        finishGame(resultBoard, true);
        resultDialog = waitForResult(resultBoard);
        QTest::qWait(80);
        require(resultBoard.findChildren<QDialog *>("gameResultDialog").size() == 1,
                "duplicate/stale result popup");
        QTest::keyClick(resultDialog, Qt::Key_Escape);
        QTest::qWait(30);
        require(resultBoard.m_bIsOver && resultBoard.m_ChessSteps.size() == 1, "Escape cleared final board");
        const QPixmap exampleEmblem =
            qvariant_cast<QPixmap>(resultBoard.findChild<QLabel *>("blackAvatar")->property("pixmap"));
        resultBoard.close();
        GameResultDialog example(false, QStringLiteral("电脑"), QStringLiteral("人机对弈 · 中等"),
                                 QStringLiteral("00:06:18"), 42, exampleEmblem);
        example.show();
        QTest::qWait(30);
        require(example.findChild<QPushButton *>("resultReview")->isDefault(),
                "review is not keyboard default");
        example.grab().save(output + "result-black.png");
        example.close();
        PuzzleGame resultPuzzle(getBuiltinPuzzles().front(), AIDifficulty::Easy);
        resultPuzzle.show();
        QTest::qWait(30);
        finishGame(resultPuzzle, true);
        waitForResult(resultPuzzle)->findChild<QPushButton *>("resultAgain")->click();
        QTest::qWait(50);
        int puzzleAlive = 0;
        for (const auto &piece : resultPuzzle.m_ChessPieces)
            puzzleAlive += !piece.m_bDead;
        require(puzzleAlive == getBuiltinPuzzles().front().pieces.size() &&
                    resultPuzzle.m_ChessSteps.isEmpty(),
                "result replay lost puzzle layout");
        resultPuzzle.close();
        menu.show();
        QTimer::singleShot(30, &menu, [] {
            if (auto *page = QApplication::activeModalWidget())
                if (auto *start = page->findChild<QPushButton *>("matchSetupStart")) start->click();
        });
        modeButtons[0]->click();
        QTest::qWait(30);
        ChessBoard *menuGame = nullptr;
        for (QWidget *widget : QApplication::topLevelWidgets())
        {
            auto *candidate = qobject_cast<ChessBoard *>(widget);
            if (candidate && candidate->isVisible())
                menuGame = candidate;
        }
        require(menuGame, "menu game unavailable for result test");
        QPointer<ChessBoard> menuGameGuard(menuGame);
        finishGame(*menuGame, false);
        waitForResult(*menuGame)->findChild<QPushButton *>("resultMenu")->click();
        QTest::qWait(60);
        require(menu.isVisible() && menuGameGuard.isNull(), "result return-to-menu did not close game");
        menu.close();
        std::cout << "PASS result popup red/black, live stats, Enter/Escape review, replay, puzzle replay, "
                     "menu, stale-dialog protection"
                  << std::endl;
        // Compare the new search at the same completed depth as the old medium search.
        ChessSearch::Position position;
        game.doMoveStone(27, -1, 5, 8);
        std::copy(std::begin(game.m_ChessPieces), std::end(game.m_ChessPieces), position.begin());
        ChessSearch search(position, false, false, true, std::make_shared<std::atomic_bool>(false));
        const auto result = search.bestMove(3, 20000);
        require(result.completedDepth == 3, "medium depth did not complete");
        std::cout << "optimized_medium_ms=" << result.elapsedMs << " nodes=" << result.nodes << std::endl;
        auto canceled = std::make_shared<std::atomic_bool>(false);
        ChessSearch bounded(position, false, false, true, canceled);
        const auto boundedResult = bounded.bestMove(20, 30);
        require(boundedResult.move.moveId != -1 && boundedResult.completedDepth >= 1 &&
                    boundedResult.elapsedMs < 150,
                "search time budget failed");
        canceled->store(true);
        ChessSearch stopped(position, false, false, true, canceled);
        require(stopped.bestMove(20, 2000).move.moveId == -1, "cancellation flag ignored");
        std::cout << "bounded_search_ms=" << boundedResult.elapsedMs
                  << " completed_depth=" << boundedResult.completedDepth << std::endl;
        game.init();
        QTest::qWait(20);
        MachineGame hard(AIDifficulty::Hard);
        int ticks = 0;
        QTimer pulse;
        QObject::connect(&pulse, &QTimer::timeout, [&]() { ++ticks; });
        pulse.start(10);
        QElapsedTimer elapsed;
        elapsed.start();
        const auto dispatchMs = humanMove(hard);
        std::cout << "hard_startup_and_input_ms=" << elapsed.elapsed() << std::endl;
        require(dispatchMs < 300, "mouse handler blocked during AI search");
        while (hard.m_ChessSteps.size() == 1 && elapsed.elapsed() < 3500)
            QTest::qWait(5);
        require(hard.m_ChessSteps.size() == 2, "AI did not apply exactly one reply");
        require(hard.m_bIsRed, "AI reply left wrong side to move");
        require(ticks >= 5, "UI heartbeat starved during search");
        std::cout << "hard_dispatch_ms=" << dispatchMs << " reply_ms=" << elapsed.elapsed()
                  << " ui_ticks=" << ticks << std::endl;
        hard.findChild<QPushButton *>("pushButton_back")->click();
        require(hard.m_ChessSteps.isEmpty() && hard.m_bIsRed, "completed-turn undo failed");
        humanMove(hard);
        hard.findChild<QPushButton *>("pushButton_restart")->click();
        QTest::qWait(2200);
        require(hard.m_ChessSteps.isEmpty() && hard.m_bIsRed && hard.m_ChessPieces[27].m_nRow == 6,
                "stale search changed restarted board");
        humanMove(hard);
        hard.findChild<QPushButton *>("pushButton_back")->click();
        QTest::qWait(100);
        require(hard.m_ChessSteps.isEmpty() && hard.m_bIsRed, "undo during search failed");
        auto *closing = new MachineGame(AIDifficulty::Hard);
        humanMove(*closing);
        closing->close();
        delete closing;
        QTest::qWait(100);
        const auto puzzle = getBuiltinPuzzles().front();
        PuzzleGame puzzles(puzzle, AIDifficulty::Hard);
        require(puzzles.findChild<QLabel *>("capturedByRed")->text().contains(QStringLiteral("暂无")),
                "missing puzzle pieces counted as captures");
        auto aiFirst = puzzle;
        aiFirst.redFirst = false;
        puzzles.loadPuzzle(aiFirst);
        puzzles.loadPuzzle(puzzle);
        puzzles.findChild<QPushButton *>("pushButton_restart")->click();
        QTest::qWait(100);
        int alive = 0;
        for (const auto &piece : puzzles.m_ChessPieces)
            alive += !piece.m_bDead;
        require(alive == puzzle.pieces.size() && puzzles.m_ChessSteps.isEmpty(),
                "puzzle reset/stale result failed");
        require(QThreadPool::globalInstance()->waitForDone(3000), "canceled workers did not stop");
        std::cout << "PASS mouse input, UI heartbeat, AI reply, restart, undo, close, puzzle reload"
                  << std::endl;
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAIL " << error.what() << std::endl;
        return 1;
    }
}
