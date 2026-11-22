// SPDX-License-Identifier: GPL-3.0-or-later
#include "ChessSearch.h"
#include "ChessRules.h"
#include <algorithm>
#include <climits>
#include <iterator>
#include <utility>

static const int chessScore[] = {10000, 20, 40, 60, 100, 80, 10};

namespace {
constexpr std::uint64_t turnKey = 0xd6e8feb86659fd93ULL;
constexpr std::size_t cacheSize = 65536;

// A fixed SplitMix64 seed makes incremental position keys reproducible.
std::uint64_t pieceKey(int id, int row, int col)
{
    std::uint64_t value = std::uint64_t(id * 90 + row * 9 + col + 1) + 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}
}

// 位置评估表（黑方视角，row 0-9, col 0-8）
// 马的位置分
static const int posMA[10][9] = {
    { 0, 0, 4, 2, 2, 2, 4, 0, 0},
    { 0, 4, 6, 8, 8, 8, 6, 4, 0},
    { 4, 6,10,12,12,12,10, 6, 4},
    { 2, 8,12,14,14,14,12, 8, 2},
    { 2, 8,12,14,16,14,12, 8, 2},
    { 2, 8,12,14,16,14,12, 8, 2},
    { 2, 8,12,14,14,14,12, 8, 2},
    { 4, 6,10,12,12,12,10, 6, 4},
    { 0, 4, 6, 8, 8, 8, 6, 4, 0},
    { 0, 0, 4, 2, 2, 2, 4, 0, 0}
};

// 车的位置分
static const int posCHE[10][9] = {
    { 6, 8, 6, 8,10, 8, 6, 8, 6},
    { 6, 8, 8,10,12,10, 8, 8, 6},
    { 4, 8, 8, 8,10, 8, 8, 8, 4},
    { 4, 8, 8, 8,10, 8, 8, 8, 4},
    { 4, 8, 8, 8,10, 8, 8, 8, 4},
    { 4, 8, 8, 8,10, 8, 8, 8, 4},
    { 4, 8, 8, 8,10, 8, 8, 8, 4},
    { 4, 8, 8, 8,10, 8, 8, 8, 4},
    { 6, 8, 8,10,12,10, 8, 8, 6},
    { 6, 8, 6, 8,10, 8, 6, 8, 6}
};

// 炮的位置分
static const int posPAO[10][9] = {
    { 0, 0, 2, 4, 4, 4, 2, 0, 0},
    { 0, 2, 4, 4, 4, 4, 4, 2, 0},
    { 2, 4, 4, 4, 6, 4, 4, 4, 2},
    { 2, 4, 6, 8, 8, 8, 6, 4, 2},
    { 2, 4, 6, 8,10, 8, 6, 4, 2},
    { 2, 4, 6, 8,10, 8, 6, 4, 2},
    { 2, 4, 6, 8, 8, 8, 6, 4, 2},
    { 2, 4, 4, 4, 6, 4, 4, 4, 2},
    { 0, 2, 4, 4, 4, 4, 4, 2, 0},
    { 0, 0, 2, 4, 4, 4, 2, 0, 0}
};

// 兵的位置分（黑方兵视角）
static const int posBING[10][9] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0},
    { 0, 0, 0, 0, 0, 0, 0, 0, 0},
    { 0, 0, 0, 0, 0, 0, 0, 0, 0},
    { 2, 2, 4, 6, 6, 6, 4, 2, 2},
    { 4, 6, 8,10,12,10, 8, 6, 4},
    { 4, 8,10,14,16,14,10, 8, 4},
    { 8,12,14,18,20,18,14,12, 8},
    {10,14,16,20,22,20,16,14,10},
    {10,14,16,20,22,20,16,14,10},
    { 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

static int getPositionScore(int type, int row, int col, bool isRed)
{
    // 红方棋子翻转行坐标
    int r = isRed ? (9 - row) : row;
    int c = isRed ? (8 - col) : col;
    switch (type) {
    case 3: return posMA[r][c];    // MA
    case 4: return posCHE[r][c];   // CHE
    case 5: return posPAO[r][c];   // PAO
    case 6: return posBING[r][c];  // BING
    default: return 0;
    }
}

int ChessSearch::evaluate(const ChessPieces* pieces, bool aiRed, bool usePositionScores)
{
    int blackScore = 0;
    int redScore = 0;

    for (int i = 0; i < 16; i++) {
        if (pieces[i].m_bDead) continue;
        int type = pieces[i].m_emType;
        blackScore += chessScore[type];
        if (usePositionScores)
            blackScore += getPositionScore(type, pieces[i].m_nRow, pieces[i].m_nCol, false);
    }

    for (int i = 16; i < 32; i++) {
        if (pieces[i].m_bDead) continue;
        int type = pieces[i].m_emType;
        redScore += chessScore[type];
        if (usePositionScores)
            redScore += getPositionScore(type, pieces[i].m_nRow, pieces[i].m_nCol, true);
    }

    // AI 执黑时返回 black-red，执红时返回 red-black
    if (aiRed)
        return redScore - blackScore;
    return blackScore - redScore;
}


ChessSearch::ChessSearch(Position position, bool redTurn, bool aiRed,
                         bool usePositionScores,
                         std::shared_ptr<std::atomic_bool> canceled, bool useTranspositions)
    : m_pieces(std::move(position)), m_redTurn(redTurn), m_aiRed(aiRed),
      m_usePositionScores(usePositionScores), m_canceled(std::move(canceled))
{
    if (useTranspositions) m_cache.resize(cacheSize);
    if (m_redTurn) m_positionKey = turnKey;
    for (auto& row : m_squares)
        std::fill(std::begin(row), std::end(row), -1);
    for (int id = 0; id < 32; ++id) {
        const auto& piece = m_pieces[id];
        if (!piece.m_bDead) {
            m_squares[piece.m_nRow][piece.m_nCol] = id;
            m_positionKey ^= pieceKey(id, piece.m_nRow, piece.m_nCol);
        }
    }
}

std::vector<SearchMove> ChessSearch::allMoves(const SearchMove &preferred) const
{
    std::vector<SearchMove> moves;
    moves.reserve(64);
    const ChessRules rules(m_pieces.data(), m_squares);
    const int start = m_redTurn ? 16 : 0;
    for (int id = start; id < start + 16; ++id) {
        const auto& piece = m_pieces[id];
        if (piece.m_bDead)
            continue;
        for (int row = 0; row < 10; ++row) {
            for (int col = 0; col < 9; ++col) {
                const int target = m_squares[row][col];
                if (rules.canMove(id, target, row, col))
                    moves.push_back({id, target, piece.m_nRow, piece.m_nCol, row, col});
            }
        }
    }
    // Most valuable victim, least valuable attacker improves alpha-beta pruning.
    const auto captureScore = [this](const SearchMove& move) {
        if (move.killId == -1)
            return 0;
        return 100000 + 16 * chessScore[m_pieces[move.killId].m_emType]
               - chessScore[m_pieces[move.moveId].m_emType];
    };
    std::stable_sort(moves.begin(), moves.end(), [&](const SearchMove& a, const SearchMove& b) {
        return captureScore(a) > captureScore(b);
    });
    const auto found = std::find_if(moves.begin(), moves.end(), [&](const SearchMove &move) {
        return move.moveId == preferred.moveId && move.rowTo == preferred.rowTo && move.colTo == preferred.colTo;
    });
    if (found != moves.end()) std::rotate(moves.begin(), found, found + 1);
    return moves;
}

void ChessSearch::makeMove(const SearchMove& move)
{
    m_positionKey ^= turnKey ^ pieceKey(move.moveId, move.rowFrom, move.colFrom)
                            ^ pieceKey(move.moveId, move.rowTo, move.colTo);
    if (move.killId != -1) m_positionKey ^= pieceKey(move.killId, move.rowTo, move.colTo);
    m_squares[move.rowFrom][move.colFrom] = -1;
    m_squares[move.rowTo][move.colTo] = move.moveId;
    auto& piece = m_pieces[move.moveId];
    piece.m_nRow = move.rowTo;
    piece.m_nCol = move.colTo;
    if (move.killId != -1)
        m_pieces[move.killId].m_bDead = true;
    m_redTurn = !m_redTurn;
}

void ChessSearch::undoMove(const SearchMove& move)
{
    m_positionKey ^= turnKey ^ pieceKey(move.moveId, move.rowFrom, move.colFrom)
                            ^ pieceKey(move.moveId, move.rowTo, move.colTo);
    if (move.killId != -1) m_positionKey ^= pieceKey(move.killId, move.rowTo, move.colTo);
    m_squares[move.rowFrom][move.colFrom] = move.moveId;
    m_squares[move.rowTo][move.colTo] = move.killId;
    auto& piece = m_pieces[move.moveId];
    piece.m_nRow = move.rowFrom;
    piece.m_nCol = move.colFrom;
    if (move.killId != -1)
        m_pieces[move.killId].m_bDead = false;
    m_redTurn = !m_redTurn;
}

bool ChessSearch::shouldStop()
{
    if (m_canceled->load(std::memory_order_relaxed) ||
        std::chrono::steady_clock::now() >= m_deadline)
        m_stopped = true;
    return m_stopped;
}

void ChessSearch::cacheResult(int depth, int score, Bound bound, const SearchMove &move)
{
    if (m_cache.empty()) return;
    m_cache[m_positionKey & (cacheSize - 1)] = {m_positionKey, move, depth, score, bound};
}

int ChessSearch::alphaBeta(int depth, int alpha, int beta, bool maximizing)
{
    ++m_nodes;
    if (shouldStop())
        return 0;
    if (m_pieces[4].m_bDead)
        return m_aiRed ? 100000 + depth : -(100000 + depth);
    if (m_pieces[20].m_bDead)
        return m_aiRed ? -(100000 + depth) : 100000 + depth;
    const int originalAlpha = alpha;
    const int originalBeta = beta;
    SearchMove preferred;
    if (!m_cache.empty()) {
        const auto entry = m_cache[m_positionKey & (cacheSize - 1)];
        if (entry.depth >= 0 && entry.key == m_positionKey) {
            preferred = entry.move;
            // Equal-depth reuse preserves the existing depth-dependent mate scores.
            if (entry.depth == depth) {
                if (entry.bound == Bound::Exact) {
                    ++m_cacheHits;
                    return entry.score;
                }
                if (entry.bound == Bound::Lower) alpha = std::max(alpha, entry.score);
                else beta = std::min(beta, entry.score);
                if (alpha >= beta) {
                    ++m_cacheHits;
                    return entry.score;
                }
            }
        }
    }
    if (depth == 0) {
        const int score = ChessRules(m_pieces.data(), m_squares).hasLegalMove(m_redTurn)
            ? evaluate(m_pieces.data(), m_aiRed, m_usePositionScores) : (maximizing ? -99999 : 99999);
        cacheResult(depth, score, Bound::Exact);
        return score;
    }

    const auto moves = allMoves(preferred);
    if (moves.empty())
        return maximizing ? -(99999 + depth) : 99999 + depth;

    int best = maximizing ? INT_MIN : INT_MAX;
    SearchMove bestMove;
    for (const auto& move : moves) {
        makeMove(move);
        const int value = alphaBeta(depth - 1, alpha, beta, !maximizing);
        undoMove(move);
        if (m_stopped)
            return 0;
        if (maximizing) {
            if (value > best) bestMove = move;
            best = std::max(best, value);
            alpha = std::max(alpha, value);
        } else {
            if (value < best) bestMove = move;
            best = std::min(best, value);
            beta = std::min(beta, value);
        }
        if (alpha >= beta)
            break;
    }
    const Bound bound = best <= originalAlpha ? Bound::Upper
        : best >= originalBeta ? Bound::Lower : Bound::Exact;
    cacheResult(depth, best, bound, bestMove);
    return best;
}

SearchResult ChessSearch::bestMove(int maxDepth, int timeLimitMs)
{
    const auto started = std::chrono::steady_clock::now();
    m_deadline = started + std::chrono::milliseconds(timeLimitMs);
    m_nodes = 0;
    m_cacheHits = 0;
    m_stopped = false;
    SearchResult result;
    if (m_canceled->load(std::memory_order_relaxed) ||
        m_pieces[4].m_bDead || m_pieces[20].m_bDead)
        return result;
    auto moves = allMoves();
    if (moves.empty())
        return result;

    // Keep a legal fallback, then publish only fully completed search depths.
    result.move = moves.front();
    for (int depth = 1; depth <= maxDepth; ++depth) {
        int bestScore = INT_MIN;
        SearchMove bestMove = result.move;
        for (const auto& move : moves) {
            if (shouldStop())
                break;
            makeMove(move);
            const int score = alphaBeta(depth - 1, bestScore, INT_MAX, false);
            undoMove(move);
            if (m_stopped)
                break;
            if (score > bestScore) {
                bestScore = score;
                bestMove = move;
            }
        }
        if (m_stopped)
            break;
        result.move = bestMove;
        result.completedDepth = depth;
        result.score = bestScore;
        const auto preferred = std::find_if(moves.begin(), moves.end(), [&](const SearchMove& move) {
            return move.moveId == bestMove.moveId && move.rowTo == bestMove.rowTo &&
                   move.colTo == bestMove.colTo;
        });
        std::rotate(moves.begin(), preferred, preferred + 1);
    }
    result.nodes = m_nodes;
    result.cacheHits = m_cacheHits;
    result.elapsedMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count());
    return result;
}
