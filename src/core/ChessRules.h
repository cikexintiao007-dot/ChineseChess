// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CHESSRULES_H
#define CHESSRULES_H

#include "ChessPieces.h"
#include <QByteArray>

// Read-only rules shared by the UI and searches on a private board snapshot.
class ChessRules
{
public:
    explicit ChessRules(const ChessPieces* pieces, const int (*squares)[9] = nullptr)
        : m_ChessPieces(pieces), m_squares(squares) {}

    bool canMove(int moveId, int killId, int row, int col) const;
    bool isInCheck(bool red) const;
    bool hasLegalMove(bool red) const;
    static QByteArray positionKey(const ChessPieces *pieces, bool redTurn);
    bool canMoveJIANG(int moveId, int killId, int row, int col) const;
    bool canMoveSHI(int moveId, int killId, int row, int col) const;
    bool canMoveXIANG(int moveId, int killId, int row, int col) const;
    bool canMoveMA(int moveId, int killId, int row, int col) const;
    bool canMoveCHE(int moveId, int killId, int row, int col) const;
    bool canMovePAO(int moveId, int killId, int row, int col) const;
    bool canMoveBING(int moveId, int killId, int row, int col) const;
    int getStoneId(int row, int col) const;
    int getStoneCountAtLine(int row1, int col1, int row2, int col2) const;
    int relation(int row1, int col1, int row2, int col2) const;
    bool isRed(int id) const { return m_ChessPieces[id].m_bRed; }
    bool isDead(int id) const { return m_ChessPieces[id].m_bDead; }

private:
    bool canMoveUnchecked(int moveId, int killId, int row, int col) const;
    int rowOf(int id) const;
    int colOf(int id) const;
    int m_moveId = -1;
    int m_capturedId = -1;
    int m_targetRow = 0;
    int m_targetCol = 0;
    const ChessPieces* m_ChessPieces;
    const int (*m_squares)[9];
};

#endif
