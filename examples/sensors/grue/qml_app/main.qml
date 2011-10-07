/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Nokia Corporation and its Subsidiary(-ies) nor
**     the names of its contributors may be used to endorse or promote
**     products derived from this software without specific prior written
**     permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtMobility.sensors 1.3
import Grue 1.0

Item {
    width: 240
    height: 330
    y: 30

    GrueSensor {
        id: sensor
        active: true
        property int lastPercent: 0
        onReadingChanged: {
            var percent = Math.floor(reading.chanceOfBeingEaten * 100);
            var thetext = "";
            if (percent == 0) {
                thetext = "It is light. You are safe from Grues.";
            } else if (lastPercent == 0) {
                thetext = "It is dark. You are likely to be eaten by a Grue.";
            }
            if (percent == 100) {
                thetext += " You have been eaten by a Grue!";
                sensor.active = false;
            } else if (percent) {
                thetext += " Your chance of being eaten by a Grue: "+percent+" percent.";
            }
            text.font.pixelSize = 30;
            text.text = "<p>" + thetext + "</p>";
            lastPercent = percent;
        }
    }

    Text {
        id: text
        anchors.fill: parent
        text: "I can't tell if you're going to be eaten by a Grue or not. You're on your own!"
        wrapMode: Text.WordWrap
        font.pixelSize: 50
    }
}
