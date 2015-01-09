/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012, 2013, 2014, 2015
 * Martin Lambers <marlam@marlam.de>
 * Frédéric Devernay <Frederic.Devernay@inrialpes.fr>
 * Joe <cuchac@email.cz>
 * Daniel Schaal <farbing@web.de>
 * D. Matz <bandregent@yahoo.de>
 * Binocle <http://binocle.com> (author: Olivier Letz <oletz@binocle.com>)
 * Frédéric Bour <frederic.bour@lakaban.net>
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

#include "config.h"

#include <QApplication>
#include <QLabel>
#include <QGridLayout>
#include <QRadioButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QDesktopWidget>
#include <QCloseEvent>

#include "fullscreendialog.h"

#include "gui_common.h"

fullscreen_dialog::fullscreen_dialog(QWidget* parent) : QWidget(parent)
{
    const int screen_count = QApplication::desktop()->screenCount();

    /* Create the dialog. This dialog differs from other dialogs (e.g. the color
     * dialog) in that its settings only take effect when the dialog is closed,
     * and not immediately. This is necessary to allow the user full flexibility
     * in defining the screens used in fullscreen mode. */

    QLabel* lbl = new QLabel(_("Configure fullscreen mode:"));
    lbl->setToolTip(_("<p>Select the screens to use in fullscreen mode.</p>"));

    _single_btn = new QRadioButton(_("Single screen:"));
    _single_btn->setToolTip(_("<p>Use a single screen for fullscreen mode.</p>"));
    _single_box = new QComboBox();
    _single_box->setToolTip(_single_btn->toolTip());
    _single_box->addItem(_("Primary screen"));
    if (screen_count > 1) {
        for (int i = 0; i < screen_count; i++)
            _single_box->addItem(str::asprintf(_("Screen %d"), i + 1).c_str());
    }

    _dual_btn = new QRadioButton(_("Dual screen:"));
    _dual_btn->setToolTip(_("<p>Use two screens for fullscreen mode.</p>"));
    _dual_box0 = new QComboBox();
    _dual_box0->setToolTip(_dual_btn->toolTip());
    _dual_box1 = new QComboBox();
    _dual_box1->setToolTip(_dual_btn->toolTip());
    if (screen_count > 1) {
        for (int i = 0; i < screen_count; i++) {
            _dual_box0->addItem(str::asprintf(_("Screen %d"), i + 1).c_str());
            _dual_box1->addItem(str::asprintf(_("Screen %d"), i + 1).c_str());
        }
    }

    _multi_btn = new QRadioButton(_("Multi screen:"));
    _multi_btn->setToolTip(_("<p>Use multiple screens for fullscreen mode.</p>"));
    _multi_edt = new QLineEdit();
    _multi_edt->setToolTip(_("<p>Comma-separated list of screens to use for fullscreen mode.</p>"));
    _multi_edt->setValidator(new QRegExpValidator(QRegExp("\\d{1,2}(,\\d{1,2}){0,15}"), 0));

    QLabel* lbl2 = new QLabel(_("When in fullscreen mode,"));
    lbl2->setToolTip(_("<p>Set special left/right view handling for fullscreen mode.</p>"));

    _flip_left_box = new QCheckBox(_("flip left view vertically."));
    _flip_left_box->setToolTip(lbl2->toolTip());
    _flop_left_box = new QCheckBox(_("flop left view horizontally."));
    _flop_left_box->setToolTip(lbl2->toolTip());
    _flip_right_box = new QCheckBox(_("flip right view vertically."));
    _flip_right_box->setToolTip(lbl2->toolTip());
    _flop_right_box = new QCheckBox(_("flop right view horizontally."));
    _flop_right_box->setToolTip(lbl2->toolTip());

    _3d_ready_sync_box = new QCheckBox(_("use DLP(R) 3-D Ready Sync"));
    _3d_ready_sync_box->setToolTip(_("<p>Use DLP&reg; 3-D Ready Sync for supported output modes.</p>"));

    _inhibit_screensaver_box = new QCheckBox(_("inhibit the screensaver"));
    _inhibit_screensaver_box->setToolTip(_("<p>Inhibit the screensaver during fullscreen playback.</p>"));

    QGridLayout *layout0 = new QGridLayout();
    layout0->addWidget(lbl, 0, 0, 1, 3);
    layout0->addWidget(_single_btn, 1, 0);
    layout0->addWidget(_single_box, 1, 1, 1, 2);
    layout0->addWidget(_dual_btn, 2, 0);
    layout0->addWidget(_dual_box0, 2, 1);
    layout0->addWidget(_dual_box1, 2, 2);
    layout0->addWidget(_multi_btn, 3, 0);
    layout0->addWidget(_multi_edt, 3, 1, 1, 2);
    layout0->addWidget(lbl2, 4, 0, 1, 3);
    layout0->addWidget(_flip_left_box, 5, 0, 1, 3);
    layout0->addWidget(_flop_left_box, 6, 0, 1, 3);
    layout0->addWidget(_flip_right_box, 7, 0, 1, 3);
    layout0->addWidget(_flop_right_box, 8, 0, 1, 3);
    layout0->addWidget(_3d_ready_sync_box, 9, 0, 1, 3);
    layout0->addWidget(_inhibit_screensaver_box, 10, 0, 1, 3);
    QGridLayout *layout1 = new QGridLayout();
    QGridLayout *layout = new QGridLayout();
    layout->addLayout(layout0, 0, 0);
    layout->addLayout(layout1, 1, 0);
    setLayout(layout);

    /* Fill in the current settings */

    if (screen_count < 3) {
        _multi_btn->setEnabled(false);
        _multi_edt->setEnabled(false);
    } else {
        _multi_edt->setText("1,2,3");
    }
    if (screen_count < 2) {
        _dual_btn->setEnabled(false);
        _dual_box0->setEnabled(false);
        _dual_box1->setEnabled(false);
    } else {
        _dual_box0->setCurrentIndex(0);
        _dual_box1->setCurrentIndex(1);
    }
    std::vector<int> conf_screens;
    for (int i = 0; i < 16; i++)
        if (dispatch::parameters().fullscreen_screens() & (1 << i))
            conf_screens.push_back(i);
    if (conf_screens.size() >= 3 && screen_count >= 3) {
        QString screen_list;
        for (size_t i = 0; i < conf_screens.size(); i++) {
            screen_list += str::from(conf_screens[i] + 1).c_str();
            if (i < conf_screens.size() - 1)
                screen_list += ',';
        }
        _multi_btn->setChecked(true);
        _multi_edt->setText(screen_list);
    } else if (conf_screens.size() == 2 && screen_count >= 2) {
        _dual_box0->setCurrentIndex(conf_screens[0]);
        _dual_box1->setCurrentIndex(conf_screens[1]);
        _dual_btn->setChecked(true);
    } else {
        if (conf_screens.size() > 0 && conf_screens[0] < screen_count)
            _single_box->setCurrentIndex(conf_screens[0] + 1);
        else
            _single_box->setCurrentIndex(0);
        _single_btn->setChecked(true);
    }
    _flip_left_box->setChecked(dispatch::parameters().fullscreen_flip_left());
    _flop_left_box->setChecked(dispatch::parameters().fullscreen_flop_left());
    _flip_right_box->setChecked(dispatch::parameters().fullscreen_flip_right());
    _flop_right_box->setChecked(dispatch::parameters().fullscreen_flop_right());
    _3d_ready_sync_box->setChecked(dispatch::parameters().fullscreen_3d_ready_sync());
#ifndef Q_OS_WIN
    _inhibit_screensaver_box->setChecked(dispatch::parameters().fullscreen_inhibit_screensaver());
#else
    _inhibit_screensaver_box->setChecked(false);
    _inhibit_screensaver_box->setEnabled(false);
#endif
}

void fullscreen_dialog::closeEvent(QCloseEvent* e)
{
    /* Activate the settings chosen in the dialog, then close. */

    // fullscreen_screens
    if (_single_btn->isChecked()) {
        if (_single_box->currentIndex() == 0)
            send_cmd(command::set_fullscreen_screens, 0);
        else
            send_cmd(command::set_fullscreen_screens, 1 << (_single_box->currentIndex() - 1));
    } else if (_dual_btn->isChecked()) {
        send_cmd(command::set_fullscreen_screens,
                (1 << _dual_box0->currentIndex()) | (1 << _dual_box1->currentIndex()));
    } else {
        int fss = 0;
        QStringList screens = _multi_edt->text().split(',', QString::SkipEmptyParts);
        for (int i = 0; i < screens.size(); i++) {
            int s = str::to<int>(screens[i].toLatin1().data());
            if (s >= 1 && s <= 16)
                fss |= (1 << (s - 1));
        }
        send_cmd(command::set_fullscreen_screens, fss);
    }
    // flip/flop settings
    send_cmd(command::set_fullscreen_flip_left, _flip_left_box->isChecked());
    send_cmd(command::set_fullscreen_flop_left, _flop_left_box->isChecked());
    send_cmd(command::set_fullscreen_flip_right, _flip_right_box->isChecked());
    send_cmd(command::set_fullscreen_flop_right, _flop_right_box->isChecked());
    // 3d ready sync
    send_cmd(command::set_fullscreen_3d_ready_sync, _3d_ready_sync_box->isChecked());
    // inhibit_screensaver
    send_cmd(command::set_fullscreen_inhibit_screensaver, _inhibit_screensaver_box->isChecked());

    e->accept();
}
