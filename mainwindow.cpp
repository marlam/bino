/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2022
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

#include <QGuiApplication>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QContextMenuEvent>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QGridLayout>
#include <QPushButton>
#include <QMediaDevices>
#include <QComboBox>
#include <QActionGroup>
#include <QMimeData>

#include "mainwindow.hpp"
#include "playlist.hpp"
#include "metadata.hpp"
#include "version.hpp"
#include "log.hpp"


QMenu* MainWindow::addBinoMenu(const QString& title)
{
    QMenu* menu = menuBar()->addMenu(title);
    _contextMenu->addMenu(menu);
    return menu;
}

void MainWindow::addBinoAction(QAction* action, QMenu* menu)
{
    menu->addAction(action);
    _widget->addAction(action);
}

MainWindow::MainWindow(Bino* bino, Widget::StereoMode stereoMode, bool fullscreen) :
    QMainWindow(),
    _bino(bino),
    _widget(new Widget(bino, stereoMode, this)),
    _contextMenu(new QMenu(this))
{
    setWindowTitle("Bino");
    setWindowIcon(QIcon(":bino-logo-small-512.png"));

    QMenu* fileMenu = addBinoMenu("&File");
    _fileOpenAction = new QAction("&Open file...", this);
    _fileOpenAction->setShortcuts({ QKeySequence::Open });
    connect(_fileOpenAction, SIGNAL(triggered()), this, SLOT(fileOpen()));
    addBinoAction(_fileOpenAction, fileMenu);
    _fileOpenURLAction = new QAction("Open &URL...", this);
    connect(_fileOpenURLAction, SIGNAL(triggered()), this, SLOT(fileOpenURL()));
    addBinoAction(_fileOpenURLAction, fileMenu);
    _fileOpenCameraAction = new QAction("Open &Camera...", this);
    connect(_fileOpenCameraAction, SIGNAL(triggered()), this, SLOT(fileOpenCamera()));
    addBinoAction(_fileOpenCameraAction, fileMenu);
    fileMenu->addSeparator();
    _fileQuitAction = new QAction("&Quit", this);
    _fileQuitAction->setShortcuts({ QKeySequence::Quit });
    connect(_fileQuitAction, SIGNAL(triggered()), this, SLOT(fileQuit()));
    addBinoAction(_fileQuitAction, fileMenu);

    _trackMenu = addBinoMenu("&Tracks");
    _trackVideoActionGroup = new QActionGroup(this);
    _trackAudioActionGroup = new QActionGroup(this);
    _trackSubtitleActionGroup = new QActionGroup(this);

    QMenu* threeDMenu = addBinoMenu("&3D Modes");
    _3d360Action = new QAction("360° mode");
    _3d360Action->setCheckable(true);
    connect(_3d360Action, SIGNAL(triggered()), this, SLOT(threeD360()));
    addBinoAction(_3d360Action, threeDMenu);
    threeDMenu->addSeparator();
    _3dInputActionGroup = new QActionGroup(this);
    QAction* threeDInMono = new QAction("Input 2D", this);
    threeDInMono->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInMono)->setData(int(VideoFrame::Layout_Mono));
    connect(threeDInMono, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInMono, threeDMenu);
    QAction* threeDInTopBottom = new QAction("Input top/bottom", this);
    threeDInTopBottom->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInTopBottom)->setData(int(VideoFrame::Layout_Top_Bottom));
    connect(threeDInTopBottom, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInTopBottom, threeDMenu);
    QAction* threeDInTopBottomHalf = new QAction("Input top/bottom half height", this);
    threeDInTopBottomHalf->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInTopBottomHalf)->setData(int(VideoFrame::Layout_Top_Bottom_Half));
    connect(threeDInTopBottomHalf, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInTopBottomHalf, threeDMenu);
    QAction* threeDInBottomTop = new QAction("Input bottom/top", this);
    threeDInBottomTop->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInBottomTop)->setData(int(VideoFrame::Layout_Bottom_Top));
    connect(threeDInBottomTop, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInBottomTop, threeDMenu);
    QAction* threeDInBottomTopHalf = new QAction("Input bottom/top half height", this);
    threeDInBottomTopHalf->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInBottomTopHalf)->setData(int(VideoFrame::Layout_Bottom_Top_Half));
    connect(threeDInBottomTopHalf, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInBottomTopHalf, threeDMenu);
    QAction* threeDInLeftRight = new QAction("Input left/right", this);
    threeDInLeftRight->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInLeftRight)->setData(int(VideoFrame::Layout_Left_Right));
    connect(threeDInLeftRight, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInLeftRight, threeDMenu);
    QAction* threeDInLeftRightHalf = new QAction("Input left/right half width", this);
    threeDInLeftRightHalf->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInLeftRightHalf)->setData(int(VideoFrame::Layout_Left_Right_Half));
    connect(threeDInLeftRightHalf, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInLeftRightHalf, threeDMenu);
    QAction* threeDInRightLeft = new QAction("Input right/left", this);
    threeDInRightLeft->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInRightLeft)->setData(int(VideoFrame::Layout_Right_Left));
    connect(threeDInRightLeft, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInRightLeft, threeDMenu);
    QAction* threeDInRightLeftHalf = new QAction("Input right/left half width", this);
    threeDInRightLeftHalf->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInRightLeftHalf)->setData(int(VideoFrame::Layout_Right_Left_Half));
    connect(threeDInRightLeftHalf, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInRightLeftHalf, threeDMenu);
    QAction* threeDInAlternatingLR = new QAction("Input alternating left-right", this);
    threeDInAlternatingLR->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInAlternatingLR)->setData(int(VideoFrame::Layout_Alternating_LR));
    connect(threeDInAlternatingLR, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInAlternatingLR, threeDMenu);
    QAction* threeDInAlternatingRL = new QAction("Input alternating right-left", this);
    threeDInAlternatingRL->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInAlternatingRL)->setData(int(VideoFrame::Layout_Alternating_RL));
    connect(threeDInAlternatingRL, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInAlternatingRL, threeDMenu);
    threeDMenu->addSeparator();
    _3dOutputActionGroup = new QActionGroup(this);
    QAction* threeDOutLeft = new QAction("Output left", this);
    threeDOutLeft->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutLeft)->setData(int(Widget::Mode_Left));
    connect(threeDOutLeft, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutLeft, threeDMenu);
    QAction* threeDOutRight = new QAction("Output right", this);
    threeDOutRight->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutRight)->setData(int(Widget::Mode_Right));
    connect(threeDOutRight, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutRight, threeDMenu);
    QAction* threeDOutStereo = new QAction("Output OpenGL Stereo", this);
    threeDOutStereo->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutStereo)->setData(int(Widget::Mode_OpenGL_Stereo));
    connect(threeDOutStereo, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutStereo, threeDMenu);
    QAction* threeDOutAlternating = new QAction("Output alternating", this);
    threeDOutAlternating->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutAlternating)->setData(int(Widget::Mode_Alternating));
    connect(threeDOutAlternating, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutAlternating, threeDMenu);
    QAction* threeDOutRCD = new QAction("Output red/cyan high quality", this);
    threeDOutRCD->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutRCD)->setData(int(Widget::Mode_Red_Cyan_Dubois));
    connect(threeDOutRCD, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutRCD, threeDMenu);
    QAction* threeDOutRCF = new QAction("Output red/cyan full color", this);
    threeDOutRCF->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutRCF)->setData(int(Widget::Mode_Red_Cyan_FullColor));
    connect(threeDOutRCF, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutRCF, threeDMenu);
    QAction* threeDOutRCH = new QAction("Output red/cyan half color", this);
    threeDOutRCH->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutRCH)->setData(int(Widget::Mode_Red_Cyan_HalfColor));
    connect(threeDOutRCH, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutRCH, threeDMenu);
    QAction* threeDOutRCM = new QAction("Output red/cyan monochrome", this);
    threeDOutRCM->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutRCM)->setData(int(Widget::Mode_Red_Cyan_Monochrome));
    connect(threeDOutRCM, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutRCM, threeDMenu);
    QAction* threeDOutGMD = new QAction("Output green/magenta high quality", this);
    threeDOutGMD->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutGMD)->setData(int(Widget::Mode_Green_Magenta_Dubois));
    connect(threeDOutGMD, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutGMD, threeDMenu);
    QAction* threeDOutGMF = new QAction("Output green/magenta full color", this);
    threeDOutGMF->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutGMF)->setData(int(Widget::Mode_Green_Magenta_FullColor));
    connect(threeDOutGMF, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutGMF, threeDMenu);
    QAction* threeDOutGMH = new QAction("Output green/magenta half color", this);
    threeDOutGMH->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutGMH)->setData(int(Widget::Mode_Green_Magenta_HalfColor));
    connect(threeDOutGMH, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutGMH, threeDMenu);
    QAction* threeDOutGMM = new QAction("Output green/magenta monochrome", this);
    threeDOutGMM->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutGMM)->setData(int(Widget::Mode_Green_Magenta_Monochrome));
    connect(threeDOutGMM, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutGMM, threeDMenu);
    QAction* threeDOutABD = new QAction("Output amber/blue high quality", this);
    threeDOutABD->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutABD)->setData(int(Widget::Mode_Amber_Blue_Dubois));
    connect(threeDOutABD, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutABD, threeDMenu);
    QAction* threeDOutABF = new QAction("Output amber/blue full color", this);
    threeDOutABF->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutABF)->setData(int(Widget::Mode_Amber_Blue_FullColor));
    connect(threeDOutABF, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutABF, threeDMenu);
    QAction* threeDOutABH = new QAction("Output amber/blue half color", this);
    threeDOutABH->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutABH)->setData(int(Widget::Mode_Amber_Blue_HalfColor));
    connect(threeDOutABH, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutABH, threeDMenu);
    QAction* threeDOutABM = new QAction("Output amber/blue monochrome", this);
    threeDOutABM->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutABM)->setData(int(Widget::Mode_Amber_Blue_Monochrome));
    connect(threeDOutABM, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutABM, threeDMenu);
    QAction* threeDOutRGM = new QAction("Output red/green monochrome", this);
    threeDOutRGM->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutRGM)->setData(int(Widget::Mode_Red_Green_Monochrome));
    connect(threeDOutRGM, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutRGM, threeDMenu);
    QAction* threeDOutRBM = new QAction("Output red/blue monochrome", this);
    threeDOutRBM->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutRBM)->setData(int(Widget::Mode_Red_Blue_Monochrome));
    connect(threeDOutRBM, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutRBM, threeDMenu);

    QMenu* mediaMenu = addBinoMenu("&Media");
    _mediaToggleVolumeMuteAction = new QAction("Mute audio", this);
    _mediaToggleVolumeMuteAction->setShortcuts({ Qt::Key_M });
    _mediaToggleVolumeMuteAction->setCheckable(true);
    connect(_mediaToggleVolumeMuteAction, SIGNAL(triggered()), this, SLOT(mediaToggleVolumeMute()));
    addBinoAction(_mediaToggleVolumeMuteAction, mediaMenu);
    _mediaVolumeIncAction = new QAction("Increase audio volume", this);
    _mediaVolumeIncAction->setShortcuts({ Qt::Key_VolumeUp });
    connect(_mediaVolumeIncAction, SIGNAL(triggered()), this, SLOT(mediaVolumeInc()));
    addBinoAction(_mediaVolumeIncAction, mediaMenu);
    _mediaVolumeDecAction = new QAction("Decrease audio volume", this);
    _mediaVolumeDecAction->setShortcuts({ Qt::Key_VolumeDown });
    connect(_mediaVolumeDecAction, SIGNAL(triggered()), this, SLOT(mediaVolumeDec()));
    addBinoAction(_mediaVolumeDecAction, mediaMenu);
    mediaMenu->addSeparator();
    _mediaTogglePauseAction = new QAction("Pause", this);
    _mediaTogglePauseAction->setShortcuts({ Qt::Key_Space });
    _mediaTogglePauseAction->setCheckable(true);
    connect(_mediaTogglePauseAction, SIGNAL(triggered()), this, SLOT(mediaTogglePause()));
    addBinoAction(_mediaTogglePauseAction, mediaMenu);
    _mediaSeekFwd1SecAction = new QAction("Seek forward 1 second", this);
    _mediaSeekFwd1SecAction->setShortcuts({ Qt::Key_Period });
    connect(_mediaSeekFwd1SecAction, SIGNAL(triggered()), this, SLOT(mediaSeekFwd1Sec()));
    addBinoAction(_mediaSeekFwd1SecAction, mediaMenu);
    _mediaSeekBwd1SecAction = new QAction("Seek backwards 1 second", this);
    _mediaSeekBwd1SecAction->setShortcuts({ Qt::Key_Comma });
    connect(_mediaSeekBwd1SecAction, SIGNAL(triggered()), this, SLOT(mediaSeekBwd1Sec()));
    addBinoAction(_mediaSeekBwd1SecAction, mediaMenu);
    _mediaSeekFwd10SecsAction = new QAction("Seek forward 10 seconds", this);
    _mediaSeekFwd10SecsAction->setShortcuts({ Qt::Key_Right });
    connect(_mediaSeekFwd10SecsAction, SIGNAL(triggered()), this, SLOT(mediaSeekFwd10Secs()));
    addBinoAction(_mediaSeekFwd10SecsAction, mediaMenu);
    _mediaSeekBwd10SecsAction = new QAction("Seek backwards 10 seconds", this);
    _mediaSeekBwd10SecsAction->setShortcuts({ Qt::Key_Left });
    connect(_mediaSeekBwd10SecsAction, SIGNAL(triggered()), this, SLOT(mediaSeekBwd10Secs()));
    addBinoAction(_mediaSeekBwd10SecsAction, mediaMenu);
    _mediaSeekFwd1MinAction = new QAction("Seek forward 1 minute", this);
    _mediaSeekFwd1MinAction->setShortcuts({ Qt::Key_Up });
    connect(_mediaSeekFwd1MinAction, SIGNAL(triggered()), this, SLOT(mediaSeekFwd1Min()));
    addBinoAction(_mediaSeekFwd1MinAction, mediaMenu);
    _mediaSeekBwd1MinAction = new QAction("Seek backwards 1 minute", this);
    _mediaSeekBwd1MinAction->setShortcuts({ Qt::Key_Down });
    connect(_mediaSeekBwd1MinAction, SIGNAL(triggered()), this, SLOT(mediaSeekBwd1Min()));
    addBinoAction(_mediaSeekBwd1MinAction, mediaMenu);
    _mediaSeekFwd10MinsAction = new QAction("Seek forward 10 minutes", this);
    _mediaSeekFwd10MinsAction->setShortcuts({ Qt::Key_PageUp });
    connect(_mediaSeekFwd10MinsAction, SIGNAL(triggered()), this, SLOT(mediaSeekFwd10Mins()));
    addBinoAction(_mediaSeekFwd10MinsAction, mediaMenu);
    _mediaSeekBwd10MinsAction = new QAction("Seek backwards 10 minutes", this);
    _mediaSeekBwd10MinsAction->setShortcuts({ Qt::Key_PageDown });
    connect(_mediaSeekBwd10MinsAction, SIGNAL(triggered()), this, SLOT(mediaSeekBwd10Mins()));
    addBinoAction(_mediaSeekBwd10MinsAction, mediaMenu);

    QMenu* viewMenu = addBinoMenu("&View");
    _viewToggleFullscreenAction = new QAction("&Fullscreen", this);
    _viewToggleFullscreenAction->setShortcuts({ QKeySequence::FullScreen });
    _viewToggleFullscreenAction->setCheckable(true);
    connect(_viewToggleFullscreenAction, SIGNAL(triggered()), this, SLOT(viewToggleFullscreen()));
    addBinoAction(_viewToggleFullscreenAction, viewMenu);
    _viewToggleSwapEyesAction = new QAction("&Swap eyes", this);
    _viewToggleSwapEyesAction->setShortcuts({ Qt::Key_F7 });
    _viewToggleSwapEyesAction->setCheckable(true);
    connect(_viewToggleSwapEyesAction, SIGNAL(triggered()), this, SLOT(viewToggleSwapEyes()));
    addBinoAction(_viewToggleSwapEyesAction, viewMenu);

    QMenu* helpMenu = addBinoMenu("&Help");
    _helpAboutAction = new QAction("&About");
    connect(_helpAboutAction, SIGNAL(triggered()), this, SLOT(helpAbout()));
    addBinoAction(_helpAboutAction, helpMenu);

    updateActions();
    connect(_bino, SIGNAL(stateChanged()), this, SLOT(updateActions()));

    connect(_widget, SIGNAL(toggleFullscreen()), this, SLOT(viewToggleFullscreen()));
    setCentralWidget(_widget);
    _widget->show();

    setMinimumSize(menuBar()->sizeHint().width(), menuBar()->sizeHint().width() / 2);
    setAcceptDrops(true);

    if (fullscreen)
        viewToggleFullscreen();
}

void MainWindow::fileOpen()
{
    QString name = QFileDialog::getOpenFileName(this);
    if (!name.isEmpty()) {
        QUrl url = QUrl::fromLocalFile(name);
        MetaData metaData;
        QString errMsg;
        if (metaData.detectCached(url, &errMsg)) {
            _bino->startPlaylistMode();
            Playlist::instance()->clear();
            Playlist::instance()->append(url);
            Playlist::instance()->start();
        } else {
            QMessageBox::critical(this, "Error", errMsg);
        }
    }
}

void MainWindow::fileOpenURL()
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Open URL");
    QLabel *label = new QLabel("URL:");
    QLineEdit *edit = new QLineEdit("");
    edit->setMinimumWidth(256);
    QPushButton *cancelBtn = new QPushButton("Cancel");
    QPushButton *okBtn = new QPushButton("OK");
    okBtn->setDefault(true);
    connect(cancelBtn, SIGNAL(clicked()), dialog, SLOT(reject()));
    connect(okBtn, SIGNAL(clicked()), dialog, SLOT(accept()));
    QGridLayout *layout = new QGridLayout();
    layout->addWidget(label, 0, 0);
    layout->addWidget(edit, 0, 1, 1, 3);
    layout->addWidget(cancelBtn, 2, 2);
    layout->addWidget(okBtn, 2, 3);
    layout->setColumnStretch(1, 1);
    dialog->setLayout(layout);
    dialog->exec();
    if (dialog->result() == QDialog::Accepted && !edit->text().isEmpty()) {
        QUrl url = QUrl::fromUserInput(edit->text());
        MetaData metaData;
        QString errMsg;
        if (metaData.detectCached(url, &errMsg)) {
            _bino->startPlaylistMode();
            Playlist::instance()->clear();
            Playlist::instance()->append(url);
            Playlist::instance()->start();
        } else {
            QMessageBox::critical(this, "Error", errMsg);
        }
    }
}

void MainWindow::fileOpenCamera()
{
    QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    QList<QAudioDevice> audioOutputDevices = QMediaDevices::audioOutputs();
    QList<QAudioDevice> audioInputDevices = QMediaDevices::audioInputs();
    QList<QCameraDevice> videoInputDevices = QMediaDevices::videoInputs();
    QGuiApplication::restoreOverrideCursor();
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Open Camera");
    QLabel *videoLabel = new QLabel("Video Input:");
    QComboBox* videoBox = new QComboBox;
    videoBox->addItem("Default");
    for (int i = 0; i < videoInputDevices.size(); i++)
        videoBox->addItem(videoInputDevices[i].description());
    QLabel *audioLabel = new QLabel("Audio Input:");
    QComboBox* audioBox = new QComboBox;
    audioBox->addItem("None");
    audioBox->addItem("Default");
    for (int i = 0; i < audioInputDevices.size(); i++)
        audioBox->addItem(audioInputDevices[i].description());
    QPushButton *cancelBtn = new QPushButton("Cancel");
    QPushButton *okBtn = new QPushButton("OK");
    okBtn->setDefault(true);
    connect(cancelBtn, SIGNAL(clicked()), dialog, SLOT(reject()));
    connect(okBtn, SIGNAL(clicked()), dialog, SLOT(accept()));
    QGridLayout *layout = new QGridLayout();
    layout->addWidget(videoLabel, 0, 0);
    layout->addWidget(videoBox, 0, 1, 1, 3);
    layout->addWidget(audioLabel, 1, 0);
    layout->addWidget(audioBox, 1, 1, 1, 3);
    layout->addWidget(cancelBtn, 2, 2);
    layout->addWidget(okBtn, 2, 3);
    layout->setColumnStretch(1, 1);
    dialog->setLayout(layout);
    dialog->exec();
    if (dialog->result() == QDialog::Accepted) {
        int videoInputDeviceIndex = videoBox->currentIndex() - 1;
        int audioInputDeviceIndex = audioBox->currentIndex() - 2;
        _bino->startCaptureMode(audioInputDeviceIndex >= -1,
                audioInputDeviceIndex >= 0
                ? audioInputDevices[audioInputDeviceIndex]
                : QMediaDevices::defaultAudioInput(),
                videoInputDeviceIndex >= 0
                ? videoInputDevices[videoInputDeviceIndex]
                : QMediaDevices::defaultVideoInput());
    }
}

void MainWindow::fileQuit()
{
    close();
}

void MainWindow::trackVideo()
{
    QAction* a = _trackVideoActionGroup->checkedAction();
    if (a)
        _bino->setVideoTrack(a->data().toInt());
}

void MainWindow::trackAudio()
{
    QAction* a = _trackAudioActionGroup->checkedAction();
    if (a)
        _bino->setAudioTrack(a->data().toInt());
}

void MainWindow::trackSubtitle()
{
    QAction* a = _trackSubtitleActionGroup->checkedAction();
    if (a)
        _bino->setSubtitleTrack(a->data().toInt());
}

void MainWindow::threeD360()
{
    _bino->setThreeSixtyMode(_3d360Action->isChecked() ? VideoFrame::ThreeSixty_On : VideoFrame::ThreeSixty_Off);
    _widget->update();
}

void MainWindow::threeDInput()
{
    QAction* a = _3dInputActionGroup->checkedAction();
    if (a) {
        _bino->setInputLayout(static_cast<VideoFrame::StereoLayout>(a->data().toInt()));
        _widget->update();
    }
}

void MainWindow::threeDOutput()
{
    QAction* a = _3dOutputActionGroup->checkedAction();
    if (a) {
        _widget->setStereoMode(static_cast<Widget::StereoMode>(a->data().toInt()));
        _widget->update();
    }
}

void MainWindow::mediaTogglePause()
{
    _bino->togglePause();
}

void MainWindow::mediaToggleVolumeMute()
{
    _bino->toggleMute();
}

void MainWindow::mediaVolumeInc()
{
    _bino->changeVolume(+0.05f);
}

void MainWindow::mediaVolumeDec()
{
    _bino->changeVolume(-0.05f);
}

void MainWindow::mediaSeekFwd1Sec()
{
    _bino->seek(+1000);
}

void MainWindow::mediaSeekBwd1Sec()
{
    _bino->seek(-1000);
}

void MainWindow::mediaSeekFwd10Secs()
{
    _bino->seek(+10000);
}

void MainWindow::mediaSeekBwd10Secs()
{
    _bino->seek(-10000);
}

void MainWindow::mediaSeekFwd1Min()
{
    _bino->seek(+60000);
}

void MainWindow::mediaSeekBwd1Min()
{
    _bino->seek(-60000);
}

void MainWindow::mediaSeekFwd10Mins()
{
    _bino->seek(+600000);
}

void MainWindow::mediaSeekBwd10Mins()
{
    _bino->seek(-600000);
}

void MainWindow::viewToggleFullscreen()
{
    if (windowState() & Qt::WindowFullScreen) {
        showNormal();
        menuBar()->show();
        activateWindow();
    } else {
        menuBar()->hide();
        showFullScreen();
        activateWindow();
    }
}

void MainWindow::viewToggleSwapEyes()
{
    _bino->toggleSwapEyes();
    _widget->update();
}

void MainWindow::updateActions()
{
    LOG_DEBUG("updating mainwindow menu state");

    _viewToggleSwapEyesAction->setChecked(_bino->swapEyes());
    _mediaTogglePauseAction->setChecked(_bino->paused());
    _mediaToggleVolumeMuteAction->setChecked(_bino->muted());

    _trackMenu->clear();
    QUrl url = _bino->url();
    MetaData metaData;
    if (!url.isEmpty() && metaData.detectCached(url)) {
        for (int i = 0; i < metaData.videoTracks.size(); i++) {
            QString s = QString("Video track %1").arg(i + 1);
            QLocale::Language l = static_cast<QLocale::Language>(metaData.videoTracks[i].value(QMediaMetaData::Language).toInt());
            if (l != QLocale::AnyLanguage)
                s += QString(" (%1)").arg(QLocale::languageToString(l));
            QAction* a = new QAction(s, this);
            a->setCheckable(true);
            _trackVideoActionGroup->addAction(a)->setData(i);
            connect(a, SIGNAL(triggered()), this, SLOT(trackVideo()));
            addBinoAction(a, _trackMenu);
            a->setChecked(_bino->videoTrack() == i);
        }
        if (metaData.videoTracks.size() > 0)
            _trackMenu->addSeparator();
        for (int i = 0; i < metaData.audioTracks.size(); i++) {
            QString s = QString("Audio track %1").arg(i + 1);
            QLocale::Language l = static_cast<QLocale::Language>(metaData.audioTracks[i].value(QMediaMetaData::Language).toInt());
            if (l != QLocale::AnyLanguage)
                s += QString(" (%1)").arg(QLocale::languageToString(l));
            QAction* a = new QAction(s, this);
            a->setCheckable(true);
            _trackAudioActionGroup->addAction(a)->setData(i);
            connect(a, SIGNAL(triggered()), this, SLOT(trackAudio()));
            addBinoAction(a, _trackMenu);
            a->setChecked(_bino->audioTrack() == i);
        }
        if (metaData.subtitleTracks.size() > 0) {
            if (metaData.audioTracks.size() > 0 || metaData.videoTracks.size() > 0)
                _trackMenu->addSeparator();
            QAction* a = new QAction("No subtitles", this);
            a->setCheckable(true);
            _trackSubtitleActionGroup->addAction(a)->setData(-1);
            connect(a, SIGNAL(triggered()), this, SLOT(trackSubtitle()));
            addBinoAction(a, _trackMenu);
            a->setChecked(_bino->subtitleTrack() < 0);
            for (int i = 0; i < metaData.subtitleTracks.size(); i++) {
                QString s = QString("Subtitle track %1").arg(i + 1);
                QLocale::Language l = static_cast<QLocale::Language>(metaData.subtitleTracks[i].value(QMediaMetaData::Language).toInt());
                if (l != QLocale::AnyLanguage)
                    s += QString(" (%1)").arg(QLocale::languageToString(l));
                QAction* a = new QAction(s, this);
                a->setCheckable(true);
                _trackSubtitleActionGroup->addAction(a)->setData(i);
                connect(a, SIGNAL(triggered()), this, SLOT(trackSubtitle()));
                addBinoAction(a, _trackMenu);
                a->setChecked(_bino->subtitleTrack() == i);
            }
        }
    } else {
        QAction* a = new QAction("None", this);
        a->setEnabled(false);
        addBinoAction(a, _trackMenu);
    }
    _trackVideoActionGroup->setEnabled(_bino->playlistMode() && !_bino->stopped());
    _trackAudioActionGroup->setEnabled(_bino->playlistMode() && !_bino->stopped());
    _trackSubtitleActionGroup->setEnabled(_bino->playlistMode() && !_bino->stopped());

    _3d360Action->setChecked(_bino->assumeThreeSixtyMode());
    VideoFrame::StereoLayout layout = _bino->assumeInputLayout();
    for (int i = 0; i < _3dInputActionGroup->actions().size(); i++) {
        QAction* a = _3dInputActionGroup->actions()[i];
        a->setChecked(a->data().toInt() == int(layout));
    }
    for (int i = 0; i < _3dOutputActionGroup->actions().size(); i++) {
        QAction* a = _3dOutputActionGroup->actions()[i];
        if (_bino->assumeStereoInputLayout()) {
            a->setEnabled(true);
            a->setChecked(a->data().toInt() == int(_widget->stereoMode()));
            Widget::StereoMode mode = static_cast<Widget::StereoMode>(a->data().toInt());
            if (mode == Widget::Mode_OpenGL_Stereo)
                a->setEnabled(_widget->isOpenGLStereo());
        } else {
            a->setEnabled(false);
            a->setChecked(false);
        }
    }

    _mediaTogglePauseAction->setEnabled(_bino->playlistMode() && !_bino->stopped());
    _mediaSeekFwd1SecAction->setEnabled(_bino->playlistMode() && !_bino->stopped());
    _mediaSeekBwd1SecAction->setEnabled(_bino->playlistMode() && !_bino->stopped());
    _mediaSeekFwd10SecsAction->setEnabled(_bino->playlistMode() && !_bino->stopped());
    _mediaSeekBwd10SecsAction->setEnabled(_bino->playlistMode() && !_bino->stopped());
    _mediaSeekFwd1MinAction->setEnabled(_bino->playlistMode() && !_bino->stopped());
    _mediaSeekBwd1MinAction->setEnabled(_bino->playlistMode() && !_bino->stopped());
    _mediaSeekFwd10MinsAction->setEnabled(_bino->playlistMode() && !_bino->stopped());
    _mediaSeekBwd10MinsAction->setEnabled(_bino->playlistMode() && !_bino->stopped());

    _widget->update();
}

void MainWindow::helpAbout()
{
    QMessageBox::about(this, "About Bino",
            QString("<p>Bino version %1<br>"
                "<a href=\"https://bino3d.org\">https://bino3d.org</a></p>"
                "<p>Copyright (C) %2 Martin Lambers<br>"
                "This is free software. You may redistribute copies of it "
                "under the terms of the <a href=\"http://www.gnu.org/licenses/gpl.html\">"
                "GNU General Public License</a>. "
                "There is NO WARRANTY, to the extent permitted by law."
                "</p>").arg(BINO_VERSION).arg(2022));
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasUrls() && event->mimeData()->urls().size() > 0) {
        QUrl url = event->mimeData()->urls()[0];
        MetaData metaData;
        QString errMsg;
        if (metaData.detectCached(url, &errMsg)) {
            _bino->startPlaylistMode();
            Playlist::instance()->clear();
            Playlist::instance()->append(url);
            Playlist::instance()->start();
        } else {
            QMessageBox::critical(this, "Error", errMsg);
        }
        event->acceptProposedAction();
    }
}

#ifndef QT_NO_CONTEXTMENU
void MainWindow::contextMenuEvent(QContextMenuEvent* event)
{
    _contextMenu->exec(event->globalPos());
}
#endif // QT_NO_CONTEXTMENU
