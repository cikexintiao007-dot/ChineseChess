// SPDX-License-Identifier: GPL-3.0-or-later
#include "ChessRules.h"
#include <cstdlib>

QByteArray ChessRules::positionKey(const ChessPieces *pieces, bool redTurn)
{
    QByteArray key(91, '\0');
    for (int id = 0; id < 32; ++id) {
        const auto &piece = pieces[id];
        if (piece.m_bDead || piece.m_nRow < 0 || piece.m_nRow > 9 || piece.m_nCol < 0 || piece.m_nCol > 8)
            continue;
        key[piece.m_nRow * 9 + piece.m_nCol] = char(1 + int(piece.m_emType) + (piece.m_bRed ? 7 : 0));
    }
    key[90] = redTurn ? 1 : 0;
    return key;
}

int ChessRules::getStoneId(int row, int col) const
{
    if (row < 0 || row >= 10 || col < 0 || col >= 9)
        return -1;
    if (m_moveId != -1) {
        if (row == m_targetRow && col == m_targetCol) return m_moveId;
        if (row == m_ChessPieces[m_moveId].m_nRow && col == m_ChessPieces[m_moveId].m_nCol) return -1;
    }
    if (m_squares)
        return m_squares[row][col];
    for(int i=0; i<32; ++i)
    {
        if(i != m_capturedId && rowOf(i) == row && colOf(i) == col && !isDead(i))
            return i;
    }

    return -1;
}

int ChessRules::getStoneCountAtLine(int row1, int col1, int row2, int col2) const
{
    int ret = 0;
    if(row1 != row2 && col1 != col2)
        return -1;
    if(row1 == row2 && col1 == col2)
        return -1;

    if(row1 == row2)
    {
        int min  = col1 < col2 ? col1 : col2;
        int max = col1 < col2 ? col2 : col1;
        for(int col = min+1; col<max; ++col)
        {
            if(getStoneId(row1, col) != -1)
                ++ret;
        }
    }
    else
    {
        int min = row1 < row2 ? row1 : row2;
        int max = row1 < row2 ? row2 : row1;
        for(int row = min+1; row<max; ++row)
        {
            if(getStoneId(row, col1) != -1)
                ++ret;
        }
    }

    return ret;
}

int ChessRules:: relation(int row1,int col1,int row2,int col2) const
{
    // 原坐标(row1,col1)与目标坐标(row2,col2)的关系
    // 使用原坐标与目标坐标的行相减的绝对值乘以10 加上原坐标与目标坐标的列相减的绝对值
    // 作为关系值
    // 关系值用于判断是否符合棋子移动规则
    return abs(row1-row2)*10+ abs(col1-col2);
}

int ChessRules::rowOf(int id) const
{
    return id == m_moveId ? m_targetRow : m_ChessPieces[id].m_nRow;
}

int ChessRules::colOf(int id) const
{
    return id == m_moveId ? m_targetCol : m_ChessPieces[id].m_nCol;
}

bool ChessRules::isInCheck(bool red) const
{
    const int king = red ? 20 : 4;
    if (isDead(king) || king == m_capturedId) return true;
    const int start = red ? 0 : 16;
    for (int id = start; id < start + 16; ++id) {
        if (!isDead(id) && id != m_capturedId
            && canMoveUnchecked(id, king, rowOf(king), colOf(king))) return true;
    }
    return false;
}

bool ChessRules::hasLegalMove(bool red) const
{
    const int start = red ? 16 : 0;
    for (int id = start; id < start + 16; ++id) {
        if (isDead(id)) continue;
        for (int row = 0; row < 10; ++row) {
            for (int col = 0; col < 9; ++col) {
                if (canMove(id, getStoneId(row, col), row, col)) return true;
            }
        }
    }
    return false;
}

bool ChessRules::canMove(int moveId, int killId, int row, int col) const
{
    if (!canMoveUnchecked(moveId, killId, row, col)) return false;
    ChessRules afterMove(m_ChessPieces, m_squares);
    afterMove.m_moveId = moveId;
    afterMove.m_capturedId = killId;
    afterMove.m_targetRow = row;
    afterMove.m_targetCol = col;
    return !afterMove.isInCheck(isRed(moveId));
}

bool ChessRules::canMoveUnchecked(int moveId, int killId, int row, int col) const
{
    if (moveId < 0 || moveId >= 32 || killId < -1 || killId >= 32 ||
        row < 0 || row >= 10 || col < 0 || col >= 9)
        return false;
    if (isDead(moveId) || moveId == m_capturedId || getStoneId(row, col) != killId)
        return false;
    if (killId != -1 && isRed(moveId) == isRed(killId))
        return false;

    switch (m_ChessPieces[moveId].m_emType)
    {
    case ChessPieces::JIANG:
        return canMoveJIANG(moveId, killId, row, col);

    case ChessPieces::SHI:
        return canMoveSHI(moveId, killId, row, col);

    case ChessPieces::XIANG:
        return canMoveXIANG(moveId, killId, row, col);

    case ChessPieces::MA:
        return canMoveMA(moveId, killId, row, col);

    case ChessPieces::CHE:
        return canMoveCHE(moveId, killId, row, col);

    case ChessPieces::PAO:
        return canMovePAO(moveId, killId, row, col);

    case ChessPieces::BING:
        return canMoveBING(moveId, killId, row, col);

    default: break;
    }

    return false;
}

bool ChessRules::canMoveJIANG(int moveId, int killId, int row, int col) const
{
    //对将的情况
    if (killId != -1 && m_ChessPieces[killId].m_emType == ChessPieces::JIANG)
        return col == colOf(moveId) && canMoveCHE(moveId, killId, row, col);

    if(isRed(moveId)) //红 将
    {
        if(row < 7 || col < 3 || col > 5) return false;
    }
    else  //黑 将
    {
        if(row > 2 || col < 3 || col > 5) return false;
    }

    int d=relation(rowOf(moveId), colOf(moveId), row, col);
    if(d == 1 || d == 10)
        return true;

    return false;
}

bool ChessRules::canMoveSHI(int moveId, int killId, int row, int col) const
{
    Q_UNUSED(killId);
    if(isRed(moveId)) //红 士
    {
        if(row < 7 || col < 3 || col > 5) return false;
    }
    else  //黑 士
    {
        if(row > 2 || col < 3 || col > 5) return false;
    }

    int d=relation(rowOf(moveId), colOf(moveId), row, col);
    if(d == 11)
        return true;

    return false;
}

bool ChessRules::canMoveXIANG(int moveId, int killId, int row, int col) const
{
    Q_UNUSED(killId);
    int d=relation(rowOf(moveId), colOf(moveId), row, col);
    if(d!= 22)
        return false;

    int row_eye= (rowOf(moveId)+ row)/ 2;
    int col_eye= (colOf(moveId)+ col)/ 2;

    //堵象眼
    if(getStoneId(row_eye,col_eye)!= -1)
        return false;

    //象不可过河
    if(isRed(moveId))   //红
    {
        if(row < 5)
            return false;
    }
    else    //黑
    {
        if(row > 4)
            return false;
    }

    return true;
}

bool ChessRules::canMoveMA(int moveId, int killId, int row, int col) const
{
    Q_UNUSED(killId);
    int d=relation(rowOf(moveId), colOf(moveId), row, col);
    if(d!=12 && d!=21)
        return false;

    //蹩马脚
    if(d==12)
    {
        if(getStoneId(rowOf(moveId), (colOf(moveId)+ col) /2) != -1)
            return false;
    }
    else
    {
        if(getStoneId((rowOf(moveId)+ row) /2 ,colOf(moveId)) != -1)
            return false;
    }

    return true;
}

bool ChessRules::canMoveCHE(int moveId, int killId, int row, int col) const
{
    Q_UNUSED(killId);
    int ret = getStoneCountAtLine(rowOf(moveId), colOf(moveId), row, col);
    if(ret == 0)
        return true;

    return false;
}

bool ChessRules::canMovePAO(int moveId, int killId, int row, int col) const
{
    int ret = getStoneCountAtLine(row, col, rowOf(moveId), colOf(moveId));
    if(killId != -1)
    {
        if(ret == 1)
            return true;
    }
    else
    {
        if(ret == 0)
            return true;
    }
    return false;
}

bool ChessRules::canMoveBING(int moveId, int killId, int row, int col) const
{
    Q_UNUSED(killId);
    int d=relation(rowOf(moveId), colOf(moveId), row, col);
    if(d!= 1 && d!= 10)
        return false;

    if(isRed(moveId))   //红
    {
        //兵卒不可后退
        if(row> rowOf(moveId))
            return false;

        //兵卒没过河不可横着走
        if(rowOf(moveId)>= 5 && rowOf(moveId)== row)
            return false;
    }
    else    //黑
    {
        if(row< rowOf(moveId))
            return false;
        if(rowOf(moveId)<= 4 && rowOf(moveId)== row)
            return false;
    }

    return true;
}
