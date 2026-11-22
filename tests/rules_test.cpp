// SPDX-License-Identifier: GPL-3.0-or-later
#include "ChessRules.h"
#include "ChessSearch.h"
#include "PuzzleData.h"
#include <iostream>
#include <stdexcept>

void require(bool condition, const char *reason)
{
    if (!condition)
        throw std::runtime_error(reason);
}

void clear(ChessPieces *pieces)
{
    for (int id = 0; id < 32; ++id)
    {
        pieces[id].init(id);
        pieces[id].m_bDead = true;
    }
}

void place(ChessPieces *pieces, int id, int row, int col)
{
    pieces[id].m_bDead = false;
    pieces[id].m_nRow = row;
    pieces[id].m_nCol = col;
}

void verifySearchCache()
{
    ChessSearch::Position position;
    for (int id = 0; id < 32; ++id) position[id].init(id);
    auto canceled = std::make_shared<std::atomic_bool>(false);
    ChessSearch baseline(position, true, true, true, canceled, false);
    ChessSearch cached(position, true, true, true, canceled);
    const auto before = baseline.bestMove(4, 15000);
    const auto after = cached.bestMove(4, 15000);
    require(before.completedDepth == 4 && after.completedDepth == 4 && before.score == after.score,
            "search cache changed fixed-depth score");
    require(after.cacheHits > 0 && before.cacheHits == 0, "search cache not exercised");
    require(ChessRules(position.data()).canMove(after.move.moveId, after.move.killId, after.move.rowTo, after.move.colTo),
            "cached search returned illegal move");
    const auto again = cached.bestMove(4, 15000);
    require(again.completedDepth == 4 && again.score == before.score && again.cacheHits > 0,
            "search position was not restored after cached traversal");
    canceled->store(true);
    require(cached.bestMove(4, 1000).move.moveId == -1, "cached search ignored cancellation");
    canceled->store(false);
    require(cached.bestMove(4, 15000).score == before.score, "search failed after cancellation reset");
    cached.bestMove(12, 1);
    const auto recovered = cached.bestMove(4, 15000);
    require(recovered.completedDepth == 4 && recovered.score == before.score,
            "partial timed search poisoned cached scores");
    auto swapped = position;
    std::swap(swapped[0].m_nRow, swapped[8].m_nRow);
    std::swap(swapped[0].m_nCol, swapped[8].m_nCol);
    require(ChessRules::positionKey(position.data(), true) == ChessRules::positionKey(swapped.data(), true),
            "identical-piece identities changed repetition key");
    require(ChessRules::positionKey(position.data(), true) != ChessRules::positionKey(position.data(), false),
            "repetition key omitted side to move");
    bool red = true;
    for (const auto &move : {SearchMove{31, -1, 6, 0, 5, 0}, SearchMove{11, -1, 3, 0, 4, 0},
                             SearchMove{31, 11, 5, 0, 4, 0}}) {
        if (move.killId != -1) position[move.killId].m_bDead = true;
        position[move.moveId].m_nRow = move.rowTo;
        position[move.moveId].m_nCol = move.colTo;
        red = !red;
        ChessSearch reference(position, red, red, true, canceled, false);
        ChessSearch optimized(position, red, red, true, canceled);
        const auto expected = reference.bestMove(3, 15000);
        const auto actual = optimized.bestMove(3, 15000);
        require(expected.completedDepth == 3 && actual.completedDepth == 3 && expected.score == actual.score,
                "cache changed score after a move/capture or side change");
        require(ChessRules(position.data()).canMove(actual.move.moveId, actual.move.killId, actual.move.rowTo, actual.move.colTo),
                "cached capture position produced illegal move");
    }
    std::cout << "PASS search cache, baseline_ms=" << before.elapsedMs << " cached_ms=" << after.elapsedMs
              << " baseline_nodes=" << before.nodes << " cached_nodes=" << after.nodes
              << " hits=" << after.cacheHits << '\n';
}

int main()
{
    try
    {
        verifySearchCache();
        ChessPieces pieces[32];
        for (int id = 0; id < 32; ++id)
            pieces[id].init(id);
        require(ChessRules(pieces).hasLegalMove(true), "initial red position has no move");
        require(!ChessRules(pieces).isInCheck(true), "initial red king in check");
        require(!ChessRules(pieces).canMove(31, -1, 5, 2), "destination occupancy not checked");
        clear(pieces);
        place(pieces, 4, 0, 3);
        place(pieces, 20, 9, 4);
        place(pieces, 0, 0, 4);
        place(pieces, 16, 5, 4);
        require(!ChessRules(pieces).canMove(16, -1, 5, 3), "pinned rook exposed its king");
        require(ChessRules(pieces).canMove(16, 0, 0, 4), "capturing checking rook rejected");
        pieces[16].m_bDead = true;
        require(ChessRules(pieces).isInCheck(true), "rook check not detected");
        place(pieces, 16, 8, 0);
        require(!ChessRules(pieces).canMove(16, -1, 7, 0), "non-evasion accepted while in check");
        clear(pieces);
        place(pieces, 4, 0, 4);
        place(pieces, 20, 9, 4);
        place(pieces, 16, 5, 4);
        require(!ChessRules(pieces).canMove(16, -1, 5, 3), "facing generals permitted");
        pieces[16].m_bDead = true;
        require(ChessRules(pieces).isInCheck(true) && ChessRules(pieces).isInCheck(false),
                "facing generals not detected");
        require(!ChessRules(pieces).canMove(16, -1, 5, 3), "dead piece moved");
        clear(pieces);
        place(pieces, 4, 0, 3);
        place(pieces, 20, 9, 5);
        place(pieces, 18, 5, 2);
        place(pieces, 2, 4, 2);
        require(!ChessRules(pieces).canMove(18, -1, 3, 4), "red elephant crossed river");
        require(!ChessRules(pieces).canMove(2, -1, 6, 4), "black elephant crossed river");
        clear(pieces);
        place(pieces, 4, 0, 4);
        place(pieces, 20, 9, 3);
        place(pieces, 16, 1, 4);
        place(pieces, 24, 2, 3);
        place(pieces, 17, 2, 6);
        require(ChessRules(pieces).isInCheck(false), "mate position not in check");
        require(!ChessRules(pieces).hasLegalMove(false), "mate position has an escape");
        clear(pieces);
        place(pieces, 4, 0, 4);
        place(pieces, 20, 9, 3);
        place(pieces, 16, 1, 3);
        place(pieces, 24, 2, 5);
        require(!ChessRules(pieces).isInCheck(false), "stalemate incorrectly marked check");
        require(!ChessRules(pieces).hasLegalMove(false), "stalemate position has an escape");
        for (const auto &puzzle : getBuiltinPuzzles())
        {
            clear(pieces);
            bool occupied[10][9] = {};
            for (const auto &piece : puzzle.pieces)
            {
                require(piece.id >= 0 && piece.id < 32 && piece.row >= 0 && piece.row < 10 &&
                            piece.col >= 0 && piece.col < 9,
                        "invalid puzzle coordinate");
                require(!occupied[piece.row][piece.col], "overlapping puzzle pieces");
                occupied[piece.row][piece.col] = true;
                place(pieces, piece.id, piece.row, piece.col);
                const auto &placed = pieces[piece.id];
                const int relativeRow = placed.m_bRed ? 9 - piece.row : piece.row;
                if (placed.m_emType == ChessPieces::JIANG || placed.m_emType == ChessPieces::SHI)
                    require(relativeRow <= 2 && piece.col >= 3 && piece.col <= 5,
                            "puzzle piece outside palace");
                if (placed.m_emType == ChessPieces::SHI)
                    require(relativeRow == 1 ? piece.col == 4 : piece.col != 4,
                            "invalid puzzle advisor square");
                if (placed.m_emType == ChessPieces::BING)
                    require(relativeRow >= 3, "puzzle pawn behind starting line");
            }
            require(!pieces[4].m_bDead && !pieces[20].m_bDead, "puzzle king missing");
            require(!ChessRules(pieces).isInCheck(!puzzle.redFirst), "puzzle starts with opponent in check");
            require(ChessRules(pieces).hasLegalMove(puzzle.redFirst), "puzzle starts with no legal move");
        }
        ChessSearch::Position position;
        for (int id = 0; id < 32; ++id)
            position[id].init(id);
        ChessSearch search(position, true, true, true, std::make_shared<std::atomic_bool>(false));
        const auto result = search.bestMove(20, 50);
        require(result.move.moveId != -1 && result.elapsedMs < 1000, "bounded AI search failed");
        require(ChessRules(position.data())
                    .canMove(result.move.moveId, result.move.killId, result.move.rowTo, result.move.colTo),
                "AI returned illegal move");
        std::cout << "PASS legal moves, checks, pins, facing generals, river, mate, stalemate, bounded AI\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
