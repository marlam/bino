/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2017 Computer Graphics Group, University of Siegen
 * Written by Martin Lambers <martin.lambers@uni-siegen.de>
 *
 * Copyright (C) 2022
 * Martin Lambers <marlam@marlam.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string>
#include <utility>
#include <map>

#include <QDataStream>

#include "screen.hpp"
#include "log.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"


Screen::Screen()
{
    positions = {
        -1.0f, +1.0f, 0.0f,
        +1.0f, +1.0f, 0.0f,
        +1.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f
    };
    texcoords = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f
    };
    indices = { 0, 3, 1, 1, 3, 2 };
    aspectRatio = 0.0f;
    isPlanar = true;
}

Screen::Screen(const QVector3D& bottomLeftCorner,
        const QVector3D& bottomRightCorner,
        const QVector3D& topLeftCorner)
{
    QVector3D topRightCorner = bottomRightCorner + (topLeftCorner - bottomLeftCorner);
    positions = {
        topLeftCorner.x(), topLeftCorner.y(), topLeftCorner.z(),
        topRightCorner.x(), topRightCorner.y(), topRightCorner.z(),
        bottomRightCorner.x(), bottomRightCorner.y(), bottomRightCorner.z(),
        bottomLeftCorner.x(), bottomLeftCorner.y(), bottomLeftCorner.z()
    };
    texcoords = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f
    };
    indices = { 0, 3, 1, 1, 3, 2 };
    float width = (bottomRightCorner - bottomLeftCorner).length();
    float height = (topLeftCorner - bottomLeftCorner).length();
    aspectRatio = width / height;
    isPlanar = true;
}

/* Helper function to split a multiline TinyObjLoader message */
static std::vector<std::string> tinyObjMsgToLines(const std::string& s)
{
    std::vector<std::string> lines;
    size_t i = 0;
    for (;;) {
        size_t j = s.find_first_of('\n', i);
        if (j < std::string::npos) {
            lines.push_back(s.substr(i, j - i));
            i = j + 1;
        } else {
            break;
        }
    }
    return lines;
}

Screen::Screen(const QString& objFileName, const QString& shapeName, float aspectRatio)
{
    LOG_INFO("%s", qPrintable(tr("Loading screen from %1").arg(objFileName)));
    tinyobj::ObjReaderConfig conf;
    conf.triangulate = true;
    conf.vertex_color = false;
    tinyobj::ObjReader reader;
    reader.ParseFromFile(qPrintable(objFileName), conf);
    if (reader.Warning().size() > 0) {
        std::vector<std::string> lines = tinyObjMsgToLines(reader.Warning());
        for (size_t i = 0; i < lines.size(); i++)
            LOG_WARNING("  %s", qPrintable(tr("warning: %1").arg(lines[i].c_str())));
    }
    if (!reader.Valid()) {
        if (reader.Error().size() > 0) {
            std::vector<std::string> lines = tinyObjMsgToLines(reader.Error());
            for (size_t i = 0; i < lines.size(); i++)
                LOG_FATAL("  %s", qPrintable(tr("error: %1").arg(lines[i].c_str())));
        } else {
            LOG_FATAL("  %s", qPrintable(tr("unknown error")));
        }
        return;
    }

    // Read all geometry (or at least the shape with the given name)
    // and ignore all materials.
    const tinyobj::attrib_t& attrib = reader.GetAttrib();
    const std::vector<tinyobj::shape_t>& shapes = reader.GetShapes();
    std::map<std::tuple<int, int>, unsigned int> indexTupleMap;
    positions.clear();
    texcoords.clear();
    indices.clear();
    bool haveTexcoords = true;
    for (size_t s = 0; s < shapes.size(); s++) {
        if (!shapeName.isEmpty() && shapeName != shapes[s].name.c_str())
            continue;
        const tinyobj::mesh_t& mesh = shapes[s].mesh;
        for (size_t i = 0; i < mesh.indices.size(); i++) {
            const tinyobj::index_t& index = mesh.indices[i];
            int vi = index.vertex_index;
            int ti = index.texcoord_index;
            std::tuple<int, int> indexTuple = std::make_tuple(vi, ti);
            auto it = indexTupleMap.find(indexTuple);
            if (it == indexTupleMap.end()) {
                unsigned int newIndex = indexTupleMap.size();
                assert(vi >= 0);
                positions.append(attrib.vertices[3 * vi + 0]);
                positions.append(attrib.vertices[3 * vi + 1]);
                positions.append(attrib.vertices[3 * vi + 2]);
                if (ti < 0)
                    haveTexcoords = false;
                if (haveTexcoords) {
                    texcoords.append(attrib.texcoords[2 * ti + 0]);
                    texcoords.append(attrib.texcoords[2 * ti + 1]);
                }
                indices.push_back(newIndex);
                indexTupleMap.insert(std::make_pair(indexTuple, newIndex));
            } else {
                indices.push_back(it->second);
            }
        }
    }
    if (!haveTexcoords) {
        texcoords.clear();
    }

    this->aspectRatio = aspectRatio;
    isPlanar = false;
}

QDataStream &operator<<(QDataStream& ds, const Screen& s)
{
    ds << s.positions << s.texcoords << s.indices << s.aspectRatio << s.isPlanar;
    return ds;
}

QDataStream &operator>>(QDataStream& ds, Screen& s)
{
    ds >> s.positions >> s.texcoords >> s.indices >> s.aspectRatio >> s.isPlanar;
    return ds;
}
