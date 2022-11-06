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

#include "gui.hpp"
#include "playlist.hpp"
#include "metadata.hpp"
#include "version.hpp"
#include "log.hpp"


QMenu* Gui::addBinoMenu(const QString& title)
{
    QMenu* menu = menuBar()->addMenu(title);
    _contextMenu->addMenu(menu);
    return menu;
}

void Gui::addBinoAction(QAction* action, QMenu* menu)
{
    menu->addAction(action);
    _widget->addAction(action);
}

static Gui* GuiSingleton = nullptr;

Gui::Gui(OutputMode outputMode, bool fullscreen) :
    QMainWindow(),
    _widget(new Widget(outputMode, this)),
    _contextMenu(new QMenu(this))
{
    setWindowTitle("Bino");
    setWindowIcon(QIcon(":bino-logo-small-512.png"));

    QMenu* fileMenu = addBinoMenu(tr("&File"));
    _fileOpenAction = new QAction(tr("&Open file..."), this);
    _fileOpenAction->setShortcuts({ QKeySequence::Open });
    connect(_fileOpenAction, SIGNAL(triggered()), this, SLOT(fileOpen()));
    addBinoAction(_fileOpenAction, fileMenu);
    _fileOpenURLAction = new QAction(tr("Open &URL..."), this);
    connect(_fileOpenURLAction, SIGNAL(triggered()), this, SLOT(fileOpenURL()));
    addBinoAction(_fileOpenURLAction, fileMenu);
    _fileOpenCameraAction = new QAction(tr("Open &Camera..."), this);
    connect(_fileOpenCameraAction, SIGNAL(triggered()), this, SLOT(fileOpenCamera()));
    addBinoAction(_fileOpenCameraAction, fileMenu);
    fileMenu->addSeparator();
    _fileQuitAction = new QAction(tr("&Quit"), this);
    _fileQuitAction->setShortcuts({ QKeySequence::Quit });
    connect(_fileQuitAction, SIGNAL(triggered()), this, SLOT(fileQuit()));
    addBinoAction(_fileQuitAction, fileMenu);

    _trackMenu = addBinoMenu(tr("&Tracks"));
    _trackVideoActionGroup = new QActionGroup(this);
    _trackAudioActionGroup = new QActionGroup(this);
    _trackSubtitleActionGroup = new QActionGroup(this);

    QMenu* threeDMenu = addBinoMenu(tr("&3D Modes"));
    _3dThreeSixtyAction = new QAction(tr("360° mode"));
    _3dThreeSixtyAction->setCheckable(true);
    connect(_3dThreeSixtyAction, SIGNAL(triggered()), this, SLOT(threeDThreeSixty()));
    addBinoAction(_3dThreeSixtyAction, threeDMenu);
    threeDMenu->addSeparator();
    _3dInputActionGroup = new QActionGroup(this);
    QAction* threeDInMono = new QAction(tr("Input 2D"), this);
    threeDInMono->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInMono)->setData(int(Input_Mono));
    connect(threeDInMono, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInMono, threeDMenu);
    QAction* threeDInTopBottom = new QAction(tr("Input top/bottom"), this);
    threeDInTopBottom->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInTopBottom)->setData(int(Input_Top_Bottom));
    connect(threeDInTopBottom, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInTopBottom, threeDMenu);
    QAction* threeDInTopBottomHalf = new QAction(tr("Input top/bottom half height"), this);
    threeDInTopBottomHalf->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInTopBottomHalf)->setData(int(Input_Top_Bottom_Half));
    connect(threeDInTopBottomHalf, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInTopBottomHalf, threeDMenu);
    QAction* threeDInBottomTop = new QAction(tr("Input bottom/top"), this);
    threeDInBottomTop->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInBottomTop)->setData(int(Input_Bottom_Top));
    connect(threeDInBottomTop, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInBottomTop, threeDMenu);
    QAction* threeDInBottomTopHalf = new QAction(tr("Input bottom/top half height"), this);
    threeDInBottomTopHalf->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInBottomTopHalf)->setData(int(Input_Bottom_Top_Half));
    connect(threeDInBottomTopHalf, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInBottomTopHalf, threeDMenu);
    QAction* threeDInLeftRight = new QAction(tr("Input left/right"), this);
    threeDInLeftRight->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInLeftRight)->setData(int(Input_Left_Right));
    connect(threeDInLeftRight, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInLeftRight, threeDMenu);
    QAction* threeDInLeftRightHalf = new QAction(tr("Input left/right half width"), this);
    threeDInLeftRightHalf->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInLeftRightHalf)->setData(int(Input_Left_Right_Half));
    connect(threeDInLeftRightHalf, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInLeftRightHalf, threeDMenu);
    QAction* threeDInRightLeft = new QAction(tr("Input right/left"), this);
    threeDInRightLeft->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInRightLeft)->setData(int(Input_Right_Left));
    connect(threeDInRightLeft, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInRightLeft, threeDMenu);
    QAction* threeDInRightLeftHalf = new QAction(tr("Input right/left half width"), this);
    threeDInRightLeftHalf->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInRightLeftHalf)->setData(int(Input_Right_Left_Half));
    connect(threeDInRightLeftHalf, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInRightLeftHalf, threeDMenu);
    QAction* threeDInAlternatingLR = new QAction(tr("Input alternating left/right"), this);
    threeDInAlternatingLR->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInAlternatingLR)->setData(int(Input_Alternating_LR));
    connect(threeDInAlternatingLR, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInAlternatingLR, threeDMenu);
    QAction* threeDInAlternatingRL = new QAction(tr("Input alternating right/left"), this);
    threeDInAlternatingRL->setCheckable(true);
    _3dInputActionGroup->addAction(threeDInAlternatingRL)->setData(int(Input_Alternating_RL));
    connect(threeDInAlternatingRL, SIGNAL(triggered()), this, SLOT(threeDInput()));
    addBinoAction(threeDInAlternatingRL, threeDMenu);
    threeDMenu->addSeparator();
    _3dOutputActionGroup = new QActionGroup(this);
    QAction* threeDOutLeft = new QAction(tr("Output left"), this);
    threeDOutLeft->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutLeft)->setData(int(Output_Left));
    connect(threeDOutLeft, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutLeft, threeDMenu);
    QAction* threeDOutRight = new QAction(tr("Output right"), this);
    threeDOutRight->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutRight)->setData(int(Output_Right));
    connect(threeDOutRight, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutRight, threeDMenu);
    QAction* threeDOutStereo = new QAction(tr("Output OpenGL Stereo"), this);
    threeDOutStereo->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutStereo)->setData(int(Output_OpenGL_Stereo));
    connect(threeDOutStereo, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutStereo, threeDMenu);
    QAction* threeDOutAlternating = new QAction(tr("Output alternating"), this);
    threeDOutAlternating->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutAlternating)->setData(int(Output_Alternating));
    connect(threeDOutAlternating, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutAlternating, threeDMenu);
    QAction* threeDOutRCD = new QAction(tr("Output red/cyan high quality"), this);
    threeDOutRCD->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutRCD)->setData(int(Output_Red_Cyan_Dubois));
    connect(threeDOutRCD, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutRCD, threeDMenu);
    QAction* threeDOutRCF = new QAction(tr("Output red/cyan full color"), this);
    threeDOutRCF->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutRCF)->setData(int(Output_Red_Cyan_FullColor));
    connect(threeDOutRCF, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutRCF, threeDMenu);
    QAction* threeDOutRCH = new QAction(tr("Output red/cyan half color"), this);
    threeDOutRCH->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutRCH)->setData(int(Output_Red_Cyan_HalfColor));
    connect(threeDOutRCH, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutRCH, threeDMenu);
    QAction* threeDOutRCM = new QAction(tr("Output red/cyan monochrome"), this);
    threeDOutRCM->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutRCM)->setData(int(Output_Red_Cyan_Monochrome));
    connect(threeDOutRCM, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutRCM, threeDMenu);
    QAction* threeDOutGMD = new QAction(tr("Output green/magenta high quality"), this);
    threeDOutGMD->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutGMD)->setData(int(Output_Green_Magenta_Dubois));
    connect(threeDOutGMD, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutGMD, threeDMenu);
    QAction* threeDOutGMF = new QAction(tr("Output green/magenta full color"), this);
    threeDOutGMF->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutGMF)->setData(int(Output_Green_Magenta_FullColor));
    connect(threeDOutGMF, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutGMF, threeDMenu);
    QAction* threeDOutGMH = new QAction(tr("Output green/magenta half color"), this);
    threeDOutGMH->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutGMH)->setData(int(Output_Green_Magenta_HalfColor));
    connect(threeDOutGMH, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutGMH, threeDMenu);
    QAction* threeDOutGMM = new QAction(tr("Output green/magenta monochrome"), this);
    threeDOutGMM->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutGMM)->setData(int(Output_Green_Magenta_Monochrome));
    connect(threeDOutGMM, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutGMM, threeDMenu);
    QAction* threeDOutABD = new QAction(tr("Output amber/blue high quality"), this);
    threeDOutABD->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutABD)->setData(int(Output_Amber_Blue_Dubois));
    connect(threeDOutABD, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutABD, threeDMenu);
    QAction* threeDOutABF = new QAction(tr("Output amber/blue full color"), this);
    threeDOutABF->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutABF)->setData(int(Output_Amber_Blue_FullColor));
    connect(threeDOutABF, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutABF, threeDMenu);
    QAction* threeDOutABH = new QAction(tr("Output amber/blue half color"), this);
    threeDOutABH->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutABH)->setData(int(Output_Amber_Blue_HalfColor));
    connect(threeDOutABH, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutABH, threeDMenu);
    QAction* threeDOutABM = new QAction(tr("Output amber/blue monochrome"), this);
    threeDOutABM->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutABM)->setData(int(Output_Amber_Blue_Monochrome));
    connect(threeDOutABM, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutABM, threeDMenu);
    QAction* threeDOutRGM = new QAction(tr("Output red/green monochrome"), this);
    threeDOutRGM->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutRGM)->setData(int(Output_Red_Green_Monochrome));
    connect(threeDOutRGM, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutRGM, threeDMenu);
    QAction* threeDOutRBM = new QAction(tr("Output red/blue monochrome"), this);
    threeDOutRBM->setCheckable(true);
    _3dOutputActionGroup->addAction(threeDOutRBM)->setData(int(Output_Red_Blue_Monochrome));
    connect(threeDOutRBM, SIGNAL(triggered()), this, SLOT(threeDOutput()));
    addBinoAction(threeDOutRBM, threeDMenu);

    QMenu* mediaMenu = addBinoMenu(tr("&Media"));
    _mediaToggleVolumeMuteAction = new QAction(tr("Mute audio"), this);
    _mediaToggleVolumeMuteAction->setShortcuts({ Qt::Key_M });
    _mediaToggleVolumeMuteAction->setCheckable(true);
    connect(_mediaToggleVolumeMuteAction, SIGNAL(triggered()), this, SLOT(mediaToggleVolumeMute()));
    addBinoAction(_mediaToggleVolumeMuteAction, mediaMenu);
    _mediaVolumeIncAction = new QAction(tr("Increase audio volume"), this);
    _mediaVolumeIncAction->setShortcuts({ Qt::Key_VolumeUp });
    connect(_mediaVolumeIncAction, SIGNAL(triggered()), this, SLOT(mediaVolumeInc()));
    addBinoAction(_mediaVolumeIncAction, mediaMenu);
    _mediaVolumeDecAction = new QAction(tr("Decrease audio volume"), this);
    _mediaVolumeDecAction->setShortcuts({ Qt::Key_VolumeDown });
    connect(_mediaVolumeDecAction, SIGNAL(triggered()), this, SLOT(mediaVolumeDec()));
    addBinoAction(_mediaVolumeDecAction, mediaMenu);
    mediaMenu->addSeparator();
    _mediaTogglePauseAction = new QAction(tr("Pause"), this);
    _mediaTogglePauseAction->setShortcuts({ Qt::Key_Space });
    _mediaTogglePauseAction->setCheckable(true);
    connect(_mediaTogglePauseAction, SIGNAL(triggered()), this, SLOT(mediaTogglePause()));
    addBinoAction(_mediaTogglePauseAction, mediaMenu);
    _mediaSeekFwd1SecAction = new QAction(tr("Seek forward 1 second"), this);
    _mediaSeekFwd1SecAction->setShortcuts({ Qt::Key_Period });
    connect(_mediaSeekFwd1SecAction, SIGNAL(triggered()), this, SLOT(mediaSeekFwd1Sec()));
    addBinoAction(_mediaSeekFwd1SecAction, mediaMenu);
    _mediaSeekBwd1SecAction = new QAction(tr("Seek backwards 1 second"), this);
    _mediaSeekBwd1SecAction->setShortcuts({ Qt::Key_Comma });
    connect(_mediaSeekBwd1SecAction, SIGNAL(triggered()), this, SLOT(mediaSeekBwd1Sec()));
    addBinoAction(_mediaSeekBwd1SecAction, mediaMenu);
    _mediaSeekFwd10SecsAction = new QAction(tr("Seek forward 10 seconds"), this);
    _mediaSeekFwd10SecsAction->setShortcuts({ Qt::Key_Right });
    connect(_mediaSeekFwd10SecsAction, SIGNAL(triggered()), this, SLOT(mediaSeekFwd10Secs()));
    addBinoAction(_mediaSeekFwd10SecsAction, mediaMenu);
    _mediaSeekBwd10SecsAction = new QAction(tr("Seek backwards 10 seconds"), this);
    _mediaSeekBwd10SecsAction->setShortcuts({ Qt::Key_Left });
    connect(_mediaSeekBwd10SecsAction, SIGNAL(triggered()), this, SLOT(mediaSeekBwd10Secs()));
    addBinoAction(_mediaSeekBwd10SecsAction, mediaMenu);
    _mediaSeekFwd1MinAction = new QAction(tr("Seek forward 1 minute"), this);
    _mediaSeekFwd1MinAction->setShortcuts({ Qt::Key_Up });
    connect(_mediaSeekFwd1MinAction, SIGNAL(triggered()), this, SLOT(mediaSeekFwd1Min()));
    addBinoAction(_mediaSeekFwd1MinAction, mediaMenu);
    _mediaSeekBwd1MinAction = new QAction(tr("Seek backwards 1 minute"), this);
    _mediaSeekBwd1MinAction->setShortcuts({ Qt::Key_Down });
    connect(_mediaSeekBwd1MinAction, SIGNAL(triggered()), this, SLOT(mediaSeekBwd1Min()));
    addBinoAction(_mediaSeekBwd1MinAction, mediaMenu);
    _mediaSeekFwd10MinsAction = new QAction(tr("Seek forward 10 minutes"), this);
    _mediaSeekFwd10MinsAction->setShortcuts({ Qt::Key_PageUp });
    connect(_mediaSeekFwd10MinsAction, SIGNAL(triggered()), this, SLOT(mediaSeekFwd10Mins()));
    addBinoAction(_mediaSeekFwd10MinsAction, mediaMenu);
    _mediaSeekBwd10MinsAction = new QAction(tr("Seek backwards 10 minutes"), this);
    _mediaSeekBwd10MinsAction->setShortcuts({ Qt::Key_PageDown });
    connect(_mediaSeekBwd10MinsAction, SIGNAL(triggered()), this, SLOT(mediaSeekBwd10Mins()));
    addBinoAction(_mediaSeekBwd10MinsAction, mediaMenu);

    QMenu* viewMenu = addBinoMenu(tr("&View"));
    _viewToggleFullscreenAction = new QAction(tr("&Fullscreen"), this);
    _viewToggleFullscreenAction->setShortcuts({ Qt::Key_F, QKeySequence::FullScreen });
    _viewToggleFullscreenAction->setCheckable(true);
    connect(_viewToggleFullscreenAction, SIGNAL(triggered()), this, SLOT(viewToggleFullscreen()));
    addBinoAction(_viewToggleFullscreenAction, viewMenu);
    _viewToggleSwapEyesAction = new QAction(tr("&Swap eyes"), this);
    _viewToggleSwapEyesAction->setShortcuts({ Qt::Key_F7 });
    _viewToggleSwapEyesAction->setCheckable(true);
    connect(_viewToggleSwapEyesAction, SIGNAL(triggered()), this, SLOT(viewToggleSwapEyes()));
    addBinoAction(_viewToggleSwapEyesAction, viewMenu);

    QMenu* helpMenu = addBinoMenu(tr("&Help"));
    _helpAboutAction = new QAction(tr("&About..."), this);
    connect(_helpAboutAction, SIGNAL(triggered()), this, SLOT(helpAbout()));
    addBinoAction(_helpAboutAction, helpMenu);

    updateActions();
    connect(Bino::instance(), SIGNAL(stateChanged()), this, SLOT(updateActions()));

    connect(_widget, SIGNAL(toggleFullscreen()), this, SLOT(viewToggleFullscreen()));
    setCentralWidget(_widget);
    _widget->show();

    connect(Bino::instance(), SIGNAL(wantQuit()), this, SLOT(fileQuit()));

    setMinimumSize(menuBar()->sizeHint().width(), menuBar()->sizeHint().width() / 2);
    setAcceptDrops(true);

    if (fullscreen)
        viewToggleFullscreen();

    Q_ASSERT(!GuiSingleton);
    GuiSingleton = this;
}

Gui* Gui::instance()
{
    return GuiSingleton;
}

void Gui::fileOpen()
{
    QString name = QFileDialog::getOpenFileName(this);
    if (!name.isEmpty()) {
        QUrl url = QUrl::fromLocalFile(name);
        MetaData metaData;
        QString errMsg;
        if (metaData.detectCached(url, &errMsg)) {
            Bino::instance()->startPlaylistMode();
            Playlist::instance()->clear();
            Playlist::instance()->append(url);
            Playlist::instance()->start();
        } else {
            QMessageBox::critical(this, tr("Error"), errMsg);
        }
    }
}

void Gui::fileOpenURL()
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Open URL"));
    QLabel *label = new QLabel(tr("URL:"));
    QLineEdit *edit = new QLineEdit("");
    edit->setMinimumWidth(256);
    QPushButton *cancelBtn = new QPushButton(tr("Cancel"));
    QPushButton *okBtn = new QPushButton(tr("OK"));
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
            Bino::instance()->startPlaylistMode();
            Playlist::instance()->clear();
            Playlist::instance()->append(url);
            Playlist::instance()->start();
        } else {
            QMessageBox::critical(this, tr("Error"), errMsg);
        }
    }
}

void Gui::fileOpenCamera()
{
    QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    QList<QAudioDevice> audioOutputDevices = QMediaDevices::audioOutputs();
    QList<QAudioDevice> audioInputDevices = QMediaDevices::audioInputs();
    QList<QCameraDevice> videoInputDevices = QMediaDevices::videoInputs();
    QGuiApplication::restoreOverrideCursor();
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Open Camera"));
    QLabel *videoLabel = new QLabel(tr("Video Input:"));
    QComboBox* videoBox = new QComboBox;
    videoBox->addItem(tr("Default"));
    for (int i = 0; i < videoInputDevices.size(); i++)
        videoBox->addItem(videoInputDevices[i].description());
    QLabel *audioLabel = new QLabel(tr("Audio Input:"));
    QComboBox* audioBox = new QComboBox;
    audioBox->addItem(tr("None"));
    audioBox->addItem(tr("Default"));
    for (int i = 0; i < audioInputDevices.size(); i++)
        audioBox->addItem(audioInputDevices[i].description());
    QPushButton *cancelBtn = new QPushButton(tr("Cancel"));
    QPushButton *okBtn = new QPushButton(tr("OK"));
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
        Bino::instance()->startCaptureMode(audioInputDeviceIndex >= -1,
                audioInputDeviceIndex >= 0
                ? audioInputDevices[audioInputDeviceIndex]
                : QMediaDevices::defaultAudioInput(),
                videoInputDeviceIndex >= 0
                ? videoInputDevices[videoInputDeviceIndex]
                : QMediaDevices::defaultVideoInput());
    }
}

void Gui::fileQuit()
{
    close();
}

void Gui::trackVideo()
{
    QAction* a = _trackVideoActionGroup->checkedAction();
    if (a)
        Bino::instance()->setVideoTrack(a->data().toInt());
}

void Gui::trackAudio()
{
    QAction* a = _trackAudioActionGroup->checkedAction();
    if (a)
        Bino::instance()->setAudioTrack(a->data().toInt());
}

void Gui::trackSubtitle()
{
    QAction* a = _trackSubtitleActionGroup->checkedAction();
    if (a)
        Bino::instance()->setSubtitleTrack(a->data().toInt());
}

void Gui::threeDThreeSixty()
{
    Bino::instance()->setThreeSixtyMode(_3dThreeSixtyAction->isChecked() ? ThreeSixty_On : ThreeSixty_Off);
    _widget->update();
}

void Gui::threeDInput()
{
    QAction* a = _3dInputActionGroup->checkedAction();
    if (a) {
        Bino::instance()->setInputMode(static_cast<InputMode>(a->data().toInt()));
        _widget->update();
    }
}

void Gui::threeDOutput()
{
    QAction* a = _3dOutputActionGroup->checkedAction();
    if (a) {
        _widget->setOutputMode(static_cast<OutputMode>(a->data().toInt()));
        _widget->update();
    }
}

void Gui::mediaTogglePause()
{
    Bino::instance()->togglePause();
}

void Gui::mediaToggleVolumeMute()
{
    Bino::instance()->toggleMute();
}

void Gui::mediaVolumeInc()
{
    Bino::instance()->changeVolume(+0.05f);
}

void Gui::mediaVolumeDec()
{
    Bino::instance()->changeVolume(-0.05f);
}

void Gui::mediaSeekFwd1Sec()
{
    Bino::instance()->seek(+1000);
}

void Gui::mediaSeekBwd1Sec()
{
    Bino::instance()->seek(-1000);
}

void Gui::mediaSeekFwd10Secs()
{
    Bino::instance()->seek(+10000);
}

void Gui::mediaSeekBwd10Secs()
{
    Bino::instance()->seek(-10000);
}

void Gui::mediaSeekFwd1Min()
{
    Bino::instance()->seek(+60000);
}

void Gui::mediaSeekBwd1Min()
{
    Bino::instance()->seek(-60000);
}

void Gui::mediaSeekFwd10Mins()
{
    Bino::instance()->seek(+600000);
}

void Gui::mediaSeekBwd10Mins()
{
    Bino::instance()->seek(-600000);
}

void Gui::viewToggleFullscreen()
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

void Gui::viewToggleSwapEyes()
{
    Bino::instance()->toggleSwapEyes();
    _widget->update();
}

void Gui::helpAbout()
{
    QMessageBox::about(this, tr("About Bino"),
            QString("<p>")
            + tr("Bino version %1").arg(BINO_VERSION)
            + QString("<br>")
            + QString("<a href=\"https://bino3d.org\">https://bino3d.org</a>")
            + QString("</p><p>")
            + tr("Copyright (C) %1 Martin Lambers").arg(2022)
            + QString("<br>")
            + tr("This is free software. You may redistribute copies of it "
                "under the terms of the <a href=\"http://www.gnu.org/licenses/gpl.html\">"
                "GNU General Public License</a>. "
                "There is NO WARRANTY, to the extent permitted by law.")
            + QString("</p>"));
}

void Gui::updateActions()
{
    LOG_DEBUG("updating Gui menu state");

    _viewToggleSwapEyesAction->setChecked(Bino::instance()->swapEyes());
    _mediaTogglePauseAction->setChecked(Bino::instance()->paused());
    _mediaToggleVolumeMuteAction->setChecked(Bino::instance()->muted());

    _trackMenu->clear();
    QUrl url = Bino::instance()->url();
    MetaData metaData;
    if (!url.isEmpty() && metaData.detectCached(url)) {
        for (int i = 0; i < metaData.videoTracks.size(); i++) {
            QString s = QString(tr("Video track %1")).arg(i + 1);
            QLocale::Language l = static_cast<QLocale::Language>(metaData.videoTracks[i].value(QMediaMetaData::Language).toInt());
            if (l != QLocale::AnyLanguage)
                s += QString(tr(" (%1)")).arg(QLocale::languageToString(l));
            QAction* a = new QAction(s, this);
            a->setCheckable(true);
            _trackVideoActionGroup->addAction(a)->setData(i);
            connect(a, SIGNAL(triggered()), this, SLOT(trackVideo()));
            addBinoAction(a, _trackMenu);
            a->setChecked(Bino::instance()->videoTrack() == i);
        }
        if (metaData.videoTracks.size() > 0)
            _trackMenu->addSeparator();
        for (int i = 0; i < metaData.audioTracks.size(); i++) {
            QString s = QString(tr("Audio track %1")).arg(i + 1);
            QLocale::Language l = static_cast<QLocale::Language>(metaData.audioTracks[i].value(QMediaMetaData::Language).toInt());
            if (l != QLocale::AnyLanguage)
                s += QString(tr(" (%1)")).arg(QLocale::languageToString(l));
            QAction* a = new QAction(s, this);
            a->setCheckable(true);
            _trackAudioActionGroup->addAction(a)->setData(i);
            connect(a, SIGNAL(triggered()), this, SLOT(trackAudio()));
            addBinoAction(a, _trackMenu);
            a->setChecked(Bino::instance()->audioTrack() == i);
        }
        if (metaData.subtitleTracks.size() > 0) {
            if (metaData.audioTracks.size() > 0 || metaData.videoTracks.size() > 0)
                _trackMenu->addSeparator();
            QAction* a = new QAction(tr("No subtitles"), this);
            a->setCheckable(true);
            _trackSubtitleActionGroup->addAction(a)->setData(-1);
            connect(a, SIGNAL(triggered()), this, SLOT(trackSubtitle()));
            addBinoAction(a, _trackMenu);
            a->setChecked(Bino::instance()->subtitleTrack() < 0);
            for (int i = 0; i < metaData.subtitleTracks.size(); i++) {
                QString s = QString(tr("Subtitle track %1")).arg(i + 1);
                QLocale::Language l = static_cast<QLocale::Language>(metaData.subtitleTracks[i].value(QMediaMetaData::Language).toInt());
                if (l != QLocale::AnyLanguage)
                    s += QString(" (%1)").arg(QLocale::languageToString(l));
                QAction* a = new QAction(s, this);
                a->setCheckable(true);
                _trackSubtitleActionGroup->addAction(a)->setData(i);
                connect(a, SIGNAL(triggered()), this, SLOT(trackSubtitle()));
                addBinoAction(a, _trackMenu);
                a->setChecked(Bino::instance()->subtitleTrack() == i);
            }
        }
    } else {
        QAction* a = new QAction(tr("None"), this);
        a->setEnabled(false);
        addBinoAction(a, _trackMenu);
    }
    _trackVideoActionGroup->setEnabled(Bino::instance()->playlistMode() && !Bino::instance()->stopped());
    _trackAudioActionGroup->setEnabled(Bino::instance()->playlistMode() && !Bino::instance()->stopped());
    _trackSubtitleActionGroup->setEnabled(Bino::instance()->playlistMode() && !Bino::instance()->stopped());

    _3dThreeSixtyAction->setChecked(Bino::instance()->assumeThreeSixtyMode());
    InputMode mode = Bino::instance()->assumeInputMode();
    for (int i = 0; i < _3dInputActionGroup->actions().size(); i++) {
        QAction* a = _3dInputActionGroup->actions()[i];
        a->setChecked(a->data().toInt() == int(mode));
    }
    for (int i = 0; i < _3dOutputActionGroup->actions().size(); i++) {
        QAction* a = _3dOutputActionGroup->actions()[i];
        if (Bino::instance()->assumeStereoInputMode()) {
            a->setEnabled(true);
            a->setChecked(a->data().toInt() == int(_widget->outputMode()));
            OutputMode outputMode = static_cast<OutputMode>(a->data().toInt());
            if (outputMode == Output_OpenGL_Stereo)
                a->setEnabled(_widget->isOpenGLStereo());
        } else {
            a->setEnabled(false);
            a->setChecked(false);
        }
    }

    _mediaTogglePauseAction->setEnabled(Bino::instance()->playlistMode() && !Bino::instance()->stopped());
    _mediaSeekFwd1SecAction->setEnabled(Bino::instance()->playlistMode() && !Bino::instance()->stopped());
    _mediaSeekBwd1SecAction->setEnabled(Bino::instance()->playlistMode() && !Bino::instance()->stopped());
    _mediaSeekFwd10SecsAction->setEnabled(Bino::instance()->playlistMode() && !Bino::instance()->stopped());
    _mediaSeekBwd10SecsAction->setEnabled(Bino::instance()->playlistMode() && !Bino::instance()->stopped());
    _mediaSeekFwd1MinAction->setEnabled(Bino::instance()->playlistMode() && !Bino::instance()->stopped());
    _mediaSeekBwd1MinAction->setEnabled(Bino::instance()->playlistMode() && !Bino::instance()->stopped());
    _mediaSeekFwd10MinsAction->setEnabled(Bino::instance()->playlistMode() && !Bino::instance()->stopped());
    _mediaSeekBwd10MinsAction->setEnabled(Bino::instance()->playlistMode() && !Bino::instance()->stopped());

    _widget->update();
}

void Gui::setOutputMode(OutputMode mode)
{
    _widget->setOutputMode(mode);
    _widget->update();
}

void Gui::setFullscreen(bool f)
{
    if (f && !(windowState() & Qt::WindowFullScreen)) {
        viewToggleFullscreen();
    } else if (!f && (windowState() & Qt::WindowFullScreen)) {
        viewToggleFullscreen();
    }
}

void Gui::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void Gui::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasUrls() && event->mimeData()->urls().size() > 0) {
        QUrl url = event->mimeData()->urls()[0];
        MetaData metaData;
        QString errMsg;
        if (metaData.detectCached(url, &errMsg)) {
            Bino::instance()->startPlaylistMode();
            Playlist::instance()->clear();
            Playlist::instance()->append(url);
            Playlist::instance()->start();
        } else {
            QMessageBox::critical(this, tr("Error"), errMsg);
        }
        event->acceptProposedAction();
    }
}

#ifndef QT_NO_CONTEXTMENU
void Gui::contextMenuEvent(QContextMenuEvent* event)
{
    _contextMenu->exec(event->globalPos());
}
#endif // QT_NO_CONTEXTMENU
