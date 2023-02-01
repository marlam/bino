/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2023
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

#include <QGridLayout>
#include <QSpacerItem>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QFileDialog>
#include <QLineEdit>

#include "playlist.hpp"
#include "playlisteditor.hpp"
#include "modes.hpp"
#include "metadata.hpp"


PlaylistEntryEditor::PlaylistEntryEditor(const PlaylistEntry& entry, QWidget* parent) :
    QDialog(parent),
    entry(entry)
{
    setModal(true);
    setWindowTitle(tr("Edit Playlist Entry"));

    QGridLayout* layout = new QGridLayout;

    QLabel* urlLabel0 = new QLabel(tr("URL:"));
    layout->addWidget(urlLabel0, 0, 0);
    urlLabel = new QLabel(entry.url.toString());
    layout->addWidget(urlLabel, 0, 1, 1, 2);

    QPushButton* setFileBtn = new QPushButton(tr("Set File..."));
    connect(setFileBtn, SIGNAL(clicked()), this, SLOT(setFile()));
    layout->addWidget(setFileBtn, 1, 1);
    QPushButton* setURLBtn = new QPushButton(tr("Set URL..."));
    connect(setURLBtn, SIGNAL(clicked()), this, SLOT(setURL()));
    layout->addWidget(setURLBtn, 1, 2);

    QLabel* inputModeLabel = new QLabel(tr("Input Mode:"));
    layout->addWidget(inputModeLabel, 2, 0);
    inputModeBox = new QComboBox(this);
    for (int i = 0; i < 12; i++)
        inputModeBox->addItem(inputModeToStringUI(static_cast<InputMode>(i)));
    connect(inputModeBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateEntry()));
    layout->addWidget(inputModeBox, 2, 1, 1, 2);

    QLabel* surroundModeLabel = new QLabel(tr("Surround Mode:"));
    layout->addWidget(surroundModeLabel, 3, 0);
    surroundModeBox = new QComboBox(this);
    for (int i = 0; i < 4; i++)
        surroundModeBox->addItem(surroundModeToStringUI(static_cast<SurroundMode>(i)));
    connect(surroundModeBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateEntry()));
    layout->addWidget(surroundModeBox, 3, 1, 1, 2);

    QLabel* videoTrackLabel = new QLabel(tr("Video Track:"));
    layout->addWidget(videoTrackLabel, 4, 0);
    videoTrackBox = new QComboBox(this);
    connect(videoTrackBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateEntry()));
    layout->addWidget(videoTrackBox, 4, 1, 1, 2);

    QLabel* audioTrackLabel = new QLabel(tr("Audio Track:"));
    layout->addWidget(audioTrackLabel, 5, 0);
    audioTrackBox = new QComboBox(this);
    connect(audioTrackBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateEntry()));
    layout->addWidget(audioTrackBox, 5, 1, 1, 2);

    QLabel* subtitleTrackLabel = new QLabel(tr("Subtitle Track:"));
    layout->addWidget(subtitleTrackLabel, 6, 0);
    subtitleTrackBox = new QComboBox(this);
    connect(subtitleTrackBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateEntry()));
    layout->addWidget(subtitleTrackBox, 6, 1, 1, 2);

    QPushButton* doneBtn = new QPushButton(tr("Done"), this);
    doneBtn->setDefault(true);
    connect(doneBtn, SIGNAL(clicked()), this, SLOT(accept()));
    layout->addWidget(doneBtn, 7, 0, 1, 3);

    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(2, 1);
    layout->setRowStretch(0, 1);
    setLayout(layout);

    updateBoxStates();
}

void PlaylistEntryEditor::setFile()
{
    QString name = QFileDialog::getOpenFileName(this);
    if (!name.isEmpty()) {
        entry.url = QUrl::fromLocalFile(name);
        urlLabel->setText(entry.url.toString());
        updateBoxStates();
        updateEntry();
    }
}

void PlaylistEntryEditor::setURL()
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
        entry.url = QUrl::fromUserInput(edit->text());
        urlLabel->setText(entry.url.toString());
        updateBoxStates();
        updateEntry();
    }
}

void PlaylistEntryEditor::updateEntry()
{
    entry.inputMode = static_cast<InputMode>(inputModeBox->currentIndex());
    entry.surroundMode = static_cast<SurroundMode>(surroundModeBox->currentIndex());
    entry.videoTrack = videoTrackBox->currentIndex() >= 0 ? videoTrackBox->currentIndex() - 1 : -1;
    entry.audioTrack = audioTrackBox->currentIndex() >= 0 ? audioTrackBox->currentIndex() - 1 : -1;
    entry.subtitleTrack = subtitleTrackBox->currentIndex() >= 0 ? subtitleTrackBox->currentIndex() - 2 : -2;
}

void PlaylistEntryEditor::updateBoxStates()
{
    MetaData metaData;
    bool haveMetaData = false;
    if (!entry.url.isEmpty()) {
        haveMetaData = metaData.detectCached(entry.url);
        if (haveMetaData && metaData.videoTracks.size() < 1)
            haveMetaData = false;
    }
    inputModeBox->setCurrentIndex(0);
    inputModeBox->setEnabled(false);
    surroundModeBox->setCurrentIndex(0);
    surroundModeBox->setEnabled(false);
    videoTrackBox->clear();
    videoTrackBox->addItem(tr("default"));
    videoTrackBox->setCurrentIndex(0);
    videoTrackBox->setEnabled(false);
    audioTrackBox->clear();
    audioTrackBox->addItem(tr("default"));
    audioTrackBox->setCurrentIndex(0);
    audioTrackBox->setEnabled(false);
    subtitleTrackBox->clear();
    subtitleTrackBox->addItem(tr("none"));
    subtitleTrackBox->setCurrentIndex(0);
    subtitleTrackBox->setEnabled(false);
    if (haveMetaData) {
        inputModeBox->setEnabled(true);
        surroundModeBox->setEnabled(true);
        for (int i = 0; i < metaData.videoTracks.size(); i++) {
            QString s = QString::number(i);
            QLocale::Language l = static_cast<QLocale::Language>(metaData.videoTracks[i].value(QMediaMetaData::Language).toInt());
            if (l != QLocale::AnyLanguage)
                s += QString(tr(" (%1)")).arg(QLocale::languageToString(l));
            videoTrackBox->addItem(s);
        }
        videoTrackBox->setEnabled(true);
        for (int i = 0; i < metaData.audioTracks.size(); i++) {
            QString s = QString::number(i);
            QLocale::Language l = static_cast<QLocale::Language>(metaData.audioTracks[i].value(QMediaMetaData::Language).toInt());
            if (l != QLocale::AnyLanguage)
                s += QString(tr(" (%1)")).arg(QLocale::languageToString(l));
            audioTrackBox->addItem(s);
        }
        audioTrackBox->setEnabled(true);
        if (metaData.subtitleTracks.size() > 0)
            subtitleTrackBox->addItem(tr("default"));
        for (int i = 0; i < metaData.subtitleTracks.size(); i++) {
            QString s = QString::number(i);
            QLocale::Language l = static_cast<QLocale::Language>(metaData.subtitleTracks[i].value(QMediaMetaData::Language).toInt());
            if (l != QLocale::AnyLanguage)
                s += QString(tr(" (%1)")).arg(QLocale::languageToString(l));
            subtitleTrackBox->addItem(s);
        }
        subtitleTrackBox->setEnabled(true);
    }
}


PlaylistEditor::PlaylistEditor(QWidget* parent) :
    QDialog(parent)
{
    setModal(true);
    setWindowTitle(tr("Edit Playlist"));

    QGridLayout* layout = new QGridLayout;

    table = new QTableWidget(this);
    table->setColumnCount(6);
    table->setHorizontalHeaderLabels({
            tr("URL"),
            tr("Input Mode"),
            tr("Surround Mode"),
            tr("Video Track"),
            tr("Audio Track"),
            tr("Subtitle Track") });
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->horizontalHeader()->setHighlightSections(false);
    table->verticalHeader()->setHighlightSections(false);
    table->resizeColumnsToContents();
    connect(table, SIGNAL(itemSelectionChanged()), this, SLOT(updateButtonState()));
    layout->addWidget(table, 0, 0, 7, 7);

    upBtn = new QPushButton(tr("Move up"), this);
    connect(upBtn, SIGNAL(clicked()), this, SLOT(up()));
    layout->addWidget(upBtn, 0, 7, 1, 1);
    downBtn = new QPushButton(tr("Move down"), this);
    connect(downBtn, SIGNAL(clicked()), this, SLOT(down()));
    layout->addWidget(downBtn, 1, 7, 1, 1);
    addBtn = new QPushButton(tr("Add..."), this);
    connect(addBtn, SIGNAL(clicked()), this, SLOT(add()));
    layout->addWidget(addBtn, 2, 7, 1, 1);
    delBtn = new QPushButton(tr("Remove"), this);
    connect(delBtn, SIGNAL(clicked()), this, SLOT(del()));
    layout->addWidget(delBtn, 3, 7, 1, 1);
    editBtn = new QPushButton(tr("Edit..."), this);
    connect(editBtn, SIGNAL(clicked()), this, SLOT(edit()));
    layout->addWidget(editBtn, 4, 7, 1, 1);
    QPushButton* doneBtn = new QPushButton(tr("Done"), this);
    doneBtn->setDefault(true);
    connect(doneBtn, SIGNAL(clicked()), this, SLOT(accept()));
    layout->addWidget(doneBtn, 5, 7, 1, 1);

    layout->addItem(new QSpacerItem(0, 0), 6, 7, 1, 1);
    layout->setColumnStretch(0, 1);
    layout->setRowStretch(0, 1);
    setLayout(layout);

    updateTable();
    updateButtonState();
}

void PlaylistEditor::updateTable()
{
    const Playlist* playlist = Playlist::instance();
    int row = table->currentRow();
    table->clearContents();
    table->setRowCount(playlist->length());
    for (int i = 0; i < playlist->length(); i++) {
        const PlaylistEntry& entry = playlist->entries()[i];
        table->setItem(i, 0, new QTableWidgetItem(entry.url.toString()));
        table->setItem(i, 1, new QTableWidgetItem(inputModeToStringUI(entry.inputMode)));
        table->setItem(i, 2, new QTableWidgetItem(surroundModeToStringUI(entry.surroundMode)));
        table->setItem(i, 3, new QTableWidgetItem(
                    entry.videoTrack < 0 ? tr("default") :
                    QString::number(entry.videoTrack)));
        table->setItem(i, 4, new QTableWidgetItem(
                    entry.audioTrack < 0 ? tr("default") :
                    QString::number(entry.audioTrack)));
        table->setItem(i, 5, new QTableWidgetItem(
                    entry.subtitleTrack < -1 ? tr("none") :
                    entry.subtitleTrack < 0 ? tr("default") :
                    QString::number(entry.subtitleTrack)));
        for (int j = 0; j < 6; j++)
            table->item(i, j)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren);
    }
    table->setCurrentCell(row, 0, QItemSelectionModel::Rows);
    table->resizeColumnsToContents();
}

void PlaylistEditor::updateButtonState()
{
    int row = selectedRow();
    bool haveSelection = (row >= 0 && row < table->rowCount());
    upBtn->setEnabled(haveSelection && row > 0);
    downBtn->setEnabled(haveSelection && row < table->rowCount() - 1);
    delBtn->setEnabled(haveSelection);
    editBtn->setEnabled(haveSelection);
}

int PlaylistEditor::selectedRow()
{
    QList<QTableWidgetItem*> selection = table->selectedItems();
    return (selection.isEmpty() ? -1 : selection[0]->row());
}

void PlaylistEditor::up()
{
    int row = selectedRow();
    if (row > 0) {
        Playlist* playlist = Playlist::instance();
        playlist->insert(row - 1, playlist->entries()[row]);
        playlist->remove(row + 1);
        updateTable();
        table->setCurrentCell(row - 1, 0);
        updateButtonState();
    }
}

void PlaylistEditor::down()
{
    int row = selectedRow();
    if (row < table->rowCount() - 1) {
        Playlist* playlist = Playlist::instance();
        playlist->insert(row + 2, playlist->entries()[row]);
        playlist->remove(row);
        updateTable();
        table->setCurrentCell(row + 1, 0);
        updateButtonState();
    }
}

void PlaylistEditor::add()
{
    Playlist* playlist = Playlist::instance();
    int row = selectedRow();
    if (row < 0 || row >= table->rowCount())
        row = playlist->length();
    playlist->insert(row, PlaylistEntry(QUrl("")));
    updateTable();
    table->setCurrentCell(row, 0);
    updateButtonState();
    edit();
}

void PlaylistEditor::del()
{
    int row = selectedRow();
    if (row >= 0 && row < table->rowCount()) {
        Playlist* playlist = Playlist::instance();
        playlist->remove(row);
        updateTable();
        table->setCurrentCell(row, 0);
        updateButtonState();
    }
}

void PlaylistEditor::edit()
{
    int row = selectedRow();
    if (row >= 0 && row < table->rowCount()) {
        Playlist* playlist = Playlist::instance();
        PlaylistEntryEditor editor(playlist->entries()[row], this);
        if (editor.exec() == QDialog::Accepted) {
            playlist->entries()[row] = editor.entry;
            updateTable();
            table->setCurrentCell(row, 0);
        }
    }
}
