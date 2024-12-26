/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2022, 2023, 2024
 * Martin Lambers <marlam@marlam.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QMainWindow>

#include "modes.hpp"
#include "widget.hpp"


class Gui : public QMainWindow
{
Q_OBJECT

private:
    Widget* _widget;

    QMenu* _contextMenu;

    QActionGroup* _playlistLoopActionGroup;
    QActionGroup* _playlistWaitActionGroup;
    QMenu* _trackMenu;
    QActionGroup* _trackVideoActionGroup;
    QActionGroup* _trackAudioActionGroup;
    QActionGroup* _trackSubtitleActionGroup;
    QActionGroup* _3dSurroundActionGroup;
    QActionGroup* _3dInputActionGroup;
    QActionGroup* _3dOutputActionGroup;
    QAction* _mediaTogglePauseAction;
    QAction* _mediaToggleVolumeMuteAction;
    QAction* _mediaVolumeIncAction;
    QAction* _mediaVolumeDecAction;
    QAction* _mediaSeekFwd1SecAction;
    QAction* _mediaSeekBwd1SecAction;
    QAction* _mediaSeekFwd10SecsAction;
    QAction* _mediaSeekBwd10SecsAction;
    QAction* _mediaSeekFwd1MinAction;
    QAction* _mediaSeekBwd1MinAction;
    QAction* _mediaSeekFwd10MinsAction;
    QAction* _mediaSeekBwd10MinsAction;
    QAction* _viewToggleFullscreenAction;
    QAction* _viewToggleSwapEyesAction;
    QAction* _viewResetSurroundAction;

    QMenu* addBinoMenu(const QString& title);
    void addBinoAction(QAction* action, QMenu* menu);

public slots:
    void fileOpen();
    void fileOpenURL();
    void fileOpenCamera();
    void fileQuit();
    void trackVideo();
    void trackAudio();
    void trackSubtitle();
    void playlistLoad();
    void playlistSave();
    void playlistEdit();
    void playlistNext();
    void playlistPrevious();
    void playlistLoop();
    void playlistWait();
    void threeDSurround();
    void threeDInput();
    void threeDOutput();
    void mediaTogglePause();
    void mediaToggleVolumeMute();
    void mediaVolumeInc();
    void mediaVolumeDec();
    void mediaSeekFwd1Sec();
    void mediaSeekBwd1Sec();
    void mediaSeekFwd10Secs();
    void mediaSeekBwd10Secs();
    void mediaSeekFwd1Min();
    void mediaSeekBwd1Min();
    void mediaSeekFwd10Mins();
    void mediaSeekBwd10Mins();
    void viewToggleFullscreen();
    void viewToggleSwapEyes();
    void viewResetSurround();
    void helpAbout();

    void updateActions();

protected:
    virtual void dragEnterEvent(QDragEnterEvent*) override;
    virtual void dropEvent(QDropEvent*) override;
#ifndef QT_NO_CONTEXTMENU
    virtual void contextMenuEvent(QContextMenuEvent* event) override;
#endif
    virtual void moveEvent(QMoveEvent*) override;

public:
    Gui(OutputMode outputMode, float surroundVerticalFOV, bool fullscreen);

    static Gui* instance();

    void setOutputMode(OutputMode mode);
    void setSurroundVerticalFieldOfView(float vfov);
    void setFullscreen(bool f);
};
