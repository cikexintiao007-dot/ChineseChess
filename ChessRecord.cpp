// SPDX-License-Identifier: GPL-3.0-or-later
#include "ChessRecord.h"
#include "ChessRules.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <cmath>
#include <utility>

namespace {
constexpr int maximumMoves = 4096;
constexpr qint64 maximumBytes = 4 * 1024 * 1024;

bool fail(QString *error, const QString &message)
{
    if (error) *error = message;
    return false;
}

bool integer(const QJsonValue &value, int minimum, int maximum, int &result)
{
    if (!value.isDouble()) return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < minimum || number > maximum || std::floor(number) != number)
        return false;
    result = static_cast<int>(number);
    return true;
}
}

ChessRecord::ChessRecord()
{
    for (int id = 0; id < 32; ++id) initial[id].init(id);
}

bool ChessRecord::validate(QString *error) const
{
    if (moves.size() > maximumMoves || seconds < 0 || seconds > 86399 || outcome < 0 || outcome > 3
        || difficulty < 0 || difficulty > 2 || mode.size() > 160 || reason.size() > 240
        || redRole.size() > 80 || blackRole.size() > 80 || description.size() > 1000)
        return fail(error, QStringLiteral("棋谱信息超出支持范围"));
    if (clockLimitMs < 0 || clockLimitMs > 3600000
        || redRemainingMs < 0 || redRemainingMs > clockLimitMs
        || blackRemainingMs < 0 || blackRemainingMs > clockLimitMs
        || (clockLimitMs > 0 && outcome == 0 && (redRemainingMs == 0 || blackRemainingMs == 0)))
        return fail(error, QStringLiteral("棋谱棋钟信息无效"));
    auto pieces = initial;
    bool occupied[10][9] = {};
    for (int id = 0; id < 32; ++id) {
        const auto &piece = pieces[id];
        ChessPieces expected;
        expected.init(id);
        if (piece.m_nID != id || piece.m_emType != expected.m_emType || piece.m_bRed != expected.m_bRed
            || piece.m_nRow < 0 || piece.m_nRow > 9 || piece.m_nCol < 0 || piece.m_nCol > 8)
            return fail(error, QStringLiteral("棋谱中的初始棋子信息无效"));
        if (piece.m_bDead) continue;
        if (occupied[piece.m_nRow][piece.m_nCol])
            return fail(error, QStringLiteral("初始局面存在重叠棋子"));
        occupied[piece.m_nRow][piece.m_nCol] = true;
    }
    if (pieces[4].m_bDead || pieces[20].m_bDead)
        return fail(error, QStringLiteral("初始局面缺少将帅"));
    bool red = redFirst;
    for (int index = 0; index < moves.size(); ++index) {
        const auto &move = moves[index];
        const ChessRules rules(pieces.data());
        if (move.id < 0 || move.id >= 32 || move.captured < -1 || move.captured >= 32
            || move.row < 0 || move.row > 9 || move.col < 0 || move.col > 8
            || pieces[4].m_bDead || pieces[20].m_bDead || pieces[move.id].m_bDead
            || pieces[move.id].m_bRed != red || rules.getStoneId(move.row, move.col) != move.captured
            || !rules.canMove(move.id, move.captured, move.row, move.col))
            return fail(error, QStringLiteral("棋谱第 %1 步不合法").arg(index + 1));
        if (move.captured != -1) pieces[move.captured].m_bDead = true;
        pieces[move.id].m_nRow = move.row;
        pieces[move.id].m_nCol = move.col;
        red = !red;
    }
    int forcedOutcome = 0;
    if (pieces[4].m_bDead) forcedOutcome = 1;
    else if (pieces[20].m_bDead) forcedOutcome = 2;
    else if (!ChessRules(pieces.data()).hasLegalMove(red)) forcedOutcome = red ? 2 : 1;
    if (forcedOutcome && outcome != forcedOutcome)
        return fail(error, QStringLiteral("棋谱结果与终局局面不一致"));
    return true;
}

bool ChessRecord::save(const QString &path, QString *error) const
{
    if (!validate(error)) return false;
    QJsonArray position;
    for (const auto &piece : initial)
        position.append(QJsonObject{{"row", piece.m_nRow}, {"col", piece.m_nCol}, {"dead", piece.m_bDead}});
    QJsonArray history;
    for (const auto &move : moves)
        history.append(QJsonObject{{"id", move.id}, {"captured", move.captured}, {"row", move.row}, {"col", move.col}});
    const QJsonObject object{{"format", "ChineseChessRecord"}, {"version", 1}, {"initial", position},
        {"moves", history}, {"redFirst", redFirst}, {"flipped", flipped}, {"computer", computer},
        {"aiRed", aiRed}, {"difficulty", difficulty}, {"seconds", seconds}, {"outcome", outcome},
        {"reason", reason}, {"mode", mode}, {"redRole", redRole}, {"blackRole", blackRole},
        {"description", description}, {"clockLimitMs", clockLimitMs},
        {"redRemainingMs", redRemainingMs}, {"blackRemainingMs", blackRemainingMs}};
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return fail(error, file.errorString());
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.commit()) return fail(error, file.errorString());
    return true;
}

bool ChessRecord::load(const QString &path, ChessRecord &record, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, file.errorString());
    if (file.size() > maximumBytes) return fail(error, QStringLiteral("棋谱文件过大"));
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.read(maximumBytes + 1), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(error, QStringLiteral("棋谱文件格式错误"));
    const auto object = document.object();
    int version;
    if (object.value("format").toString() != QStringLiteral("ChineseChessRecord")
        || !integer(object.value("version"), 1, 1, version))
        return fail(error, QStringLiteral("不支持的棋谱格式或版本"));
    ChessRecord candidate;
    if (!object.value("initial").isArray() || object.value("initial").toArray().size() != 32
        || !object.value("moves").isArray() || object.value("moves").toArray().size() > maximumMoves)
        return fail(error, QStringLiteral("棋谱局面或步数信息无效"));
    const auto position = object.value("initial").toArray();
    for (int id = 0; id < 32; ++id) {
        const auto piece = position[id].toObject();
        if (!integer(piece.value("row"), 0, 9, candidate.initial[id].m_nRow)
            || !integer(piece.value("col"), 0, 8, candidate.initial[id].m_nCol) || !piece.value("dead").isBool())
            return fail(error, QStringLiteral("棋谱棋子坐标无效"));
        candidate.initial[id].m_bDead = piece.value("dead").toBool();
    }
    for (const auto &value : object.value("moves").toArray()) {
        const auto step = value.toObject();
        RecordedMove move;
        if (!integer(step.value("id"), 0, 31, move.id) || !integer(step.value("captured"), -1, 31, move.captured)
            || !integer(step.value("row"), 0, 9, move.row) || !integer(step.value("col"), 0, 8, move.col))
            return fail(error, QStringLiteral("棋谱走棋数据无效"));
        candidate.moves.append(move);
    }
    for (const auto &key : {"redFirst", "flipped", "computer", "aiRed"})
        if (!object.value(key).isBool()) return fail(error, QStringLiteral("棋谱对局设置无效"));
    candidate.redFirst = object.value("redFirst").toBool();
    candidate.flipped = object.value("flipped").toBool();
    candidate.computer = object.value("computer").toBool();
    candidate.aiRed = object.value("aiRed").toBool();
    if (!integer(object.value("difficulty"), 0, 2, candidate.difficulty)
        || !integer(object.value("seconds"), 0, 86399, candidate.seconds)
        || !integer(object.value("outcome"), 0, 3, candidate.outcome))
        return fail(error, QStringLiteral("棋谱计时或结果信息无效"));
    if ((object.contains("clockLimitMs") || object.contains("redRemainingMs") || object.contains("blackRemainingMs"))
        && (!integer(object.value("clockLimitMs"), 0, 3600000, candidate.clockLimitMs)
            || !integer(object.value("redRemainingMs"), 0, candidate.clockLimitMs, candidate.redRemainingMs)
            || !integer(object.value("blackRemainingMs"), 0, candidate.clockLimitMs, candidate.blackRemainingMs)))
        return fail(error, QStringLiteral("棋谱棋钟信息无效"));
    for (const auto &key : {"reason", "mode", "redRole", "blackRole", "description"})
        if (!object.value(key).isString()) return fail(error, QStringLiteral("棋谱文字信息无效"));
    candidate.reason = object.value("reason").toString();
    candidate.mode = object.value("mode").toString();
    candidate.redRole = object.value("redRole").toString();
    candidate.blackRole = object.value("blackRole").toString();
    candidate.description = object.value("description").toString();
    if (!candidate.validate(error)) return false;
    record = std::move(candidate);
    return true;
}
