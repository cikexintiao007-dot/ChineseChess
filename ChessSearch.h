// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CHESSSEARCH_H
#define CHESSSEARCH_H

#include "ChessPieces.h"
#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <vector>
#include <cstdint>

struct SearchMove
{
    int moveId = -1;
    int killId = -1;
    int rowFrom = 0;
    int colFrom = 0;
    int rowTo = 0;
    int colTo = 0;
};

struct SearchResult
{
    SearchMove move;
    int completedDepth = 0;
    int elapsedMs = 0;
    unsigned long long nodes = 0;
    unsigned long long cacheHits = 0;
    int score = 0;
};

class ChessSearch
{
public:
    using Position = std::array<ChessPieces, 32>;
    ChessSearch(Position position, bool redTurn, bool aiRed, bool usePositionScores,
                std::shared_ptr<std::atomic_bool> canceled, bool useTranspositions = true);
    SearchResult bestMove(int maxDepth, int timeLimitMs);
    static int evaluate(const ChessPieces* pieces, bool aiRed, bool usePositionScores);

private:
    std::vector<SearchMove> allMoves(const SearchMove &preferred = {}) const;
    void makeMove(const SearchMove& move);
    void undoMove(const SearchMove& move);
    int alphaBeta(int depth, int alpha, int beta, bool maximizing);
    bool shouldStop();
    enum class Bound { Exact, Lower, Upper };
    struct CacheEntry
    {
        std::uint64_t key = 0;
        SearchMove move;
        int depth = -1;
        int score = 0;
        Bound bound = Bound::Exact;
    };
    void cacheResult(int depth, int score, Bound bound, const SearchMove &move = {});

    Position m_pieces;
    int m_squares[10][9];
    bool m_redTurn;
    bool m_aiRed;
    bool m_usePositionScores;
    std::shared_ptr<std::atomic_bool> m_canceled;
    std::chrono::steady_clock::time_point m_deadline;
    bool m_stopped = false;
    unsigned long long m_nodes = 0;
    unsigned long long m_cacheHits = 0;
    std::uint64_t m_positionKey = 0;
    std::vector<CacheEntry> m_cache;
};

#endif
