// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LANROOMDIALOG_H
#define LANROOMDIALOG_H
#include <QDialog>
class LanRoom;
class LanRoomDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LanRoomDialog(QWidget* parent = nullptr);
    LanRoom* takeRoom();
private:
    LanRoom* m_room;
};
#endif
