/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2024
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

#include <QDir>
#include <QTemporaryFile>
#include <QSharedPointer>
#include <QImage>
#include <cstring>

#include "urlloader.hpp"
#include "log.hpp"
#include "digestiblemedia.hpp"


/* We convert JPEGs to temporary PPMs here, for the following reasons:
 * - QtMultimedia tries to decode JPEGs with hardware acceleration, which fails
 *   when the image dimensions (or other properties) are not within the
 *   constraints of the hardware decoder, which is optimized for video.
 *   This happens often with both the GStreamer and FFmpeg backends.
 *   Unfortunately there does not seem to be a fallback to software decoding.
 * - MPO files can contain multiple JPEG images. In the only relevant use case,
 *   they contain a left and a right JPEG with a lot of junk in between (probably
 *   called metadata by some standard). These files typically cannot be read
 *   reliably by either QtMultimedia backend. So we read both JPEGs manually
 *   from the MPO and stack them on top of each other (top-bottom format).
 * - The destination image format is PPM because it is fast to write (no
 *   compression) and the media backend will not be tempted to try hardware
 *   accelerated decoding on PPMs.
 */

QUrl digestibleMediaUrl(const QUrl& url)
{
    static QMap<QUrl, QSharedPointer<QTemporaryFile>> cache;

    // check if we need conversion
    bool needsConversion = url.fileName().endsWith(".jpg", Qt::CaseInsensitive)
        || url.fileName().endsWith(".jpeg", Qt::CaseInsensitive)
        || url.fileName().endsWith(".jps", Qt::CaseInsensitive)
        || url.fileName().endsWith(".mpo", Qt::CaseInsensitive);
    if (!needsConversion) {
        LOG_DEBUG("%s", qPrintable(QString("digestibleMediaUrl: %1 needs no conversion").arg(url.toString())));
        return url;
    }

    // check if we have it cached
    QSharedPointer<QTemporaryFile> tempFile = nullptr;
    auto it = cache.find(url);
    if (it != cache.end()) {
        tempFile = it.value();
        LOG_DEBUG("%s", qPrintable(QString("digestibleMediaUrl: %1 is in cache: %2").arg(url.toString()).arg(tempFile->fileName())));
    }

    // build temporary file if it is not in cache yet
    if (!tempFile) {
        UrlLoader loader(url);
        const QByteArray& data = loader.load();
        if (data.size() == 0) {
            LOG_DEBUG("%s", qPrintable(QString("digestibleMediaUrl: %1: cannot download").arg(url.toString())));
            return url;
        }

        QImage img;
        if (!img.loadFromData(data, "JPG")) {
            LOG_DEBUG("%s", qPrintable(QString("digestibleMediaUrl: %1: cannot load JPEG").arg(url.toString())));
            return url;
        }

        if (url.fileName().endsWith("mpo", Qt::CaseInsensitive)) {
            unsigned char jpegMarker[4] = { 0xff, 0xd8, 0xff, 0xe1 };
            QByteArrayView jpegMarkerView(jpegMarker, 4);
            qsizetype nextJpeg = data.indexOf(jpegMarkerView, 4);
            if (nextJpeg <= 0) {
                LOG_DEBUG("%s", qPrintable(QString("digestibleMediaUrl: %1: no second jpeg marker found").arg(url.toString())));
            } else {
                QImage imgRight;
                if (!imgRight.loadFromData(QByteArrayView(data.data() + nextJpeg, data.size() - nextJpeg), "JPG")) {
                    LOG_DEBUG("%s", qPrintable(QString("digestibleMediaUrl: %1: cannot load second jpeg").arg(url.toString())));
                } else {
                    if (img.format() != imgRight.format() || img.size() != imgRight.size()) {
                        LOG_DEBUG("%s", qPrintable(QString("digestibleMediaUrl: %1: second jpeg is incompatible").arg(url.toString())));
                    } else {
                        QImage combinedImg(img.width(), 2 * img.height(), img.format());
                        for (int i = 0; i < img.height(); i++) {
                            std::memcpy(combinedImg.scanLine(i), img.constScanLine(i), img.bytesPerLine());
                        }
                        for (int i = 0; i < img.height(); i++) {
                            std::memcpy(combinedImg.scanLine(img.height() + i), imgRight.constScanLine(i), imgRight.bytesPerLine());
                        }
                        img = combinedImg;
                    }
                }
            }
        }

        QString tmpl = QDir::tempPath() + '/' + QString("bino-XXXXXX") + QString(".ppm");
        tempFile.reset(new QTemporaryFile(tmpl));
        if (!img.save(tempFile.get(), "PPM")) {
            LOG_DEBUG("%s", qPrintable(QString("digestibleMediaUrl: %1: cannot save to %2").arg(url.toString()).arg(tempFile->fileName())));
            return url;
        }

        LOG_DEBUG("%s", qPrintable(QString("digestibleMediaUrl: %1 is saved in %2").arg(url.toString()).arg(tempFile->fileName())));
        cache.insert(url, tempFile);
    }

    return QUrl::fromLocalFile(tempFile->fileName());
}
