/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the QtSensors module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef SIMULATORGESTURESCOMMON_H
#define SIMULATORGESTURESCOMMON_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtSimulator/connection.h>
#include <QtSimulator/connectionworker.h>
#include <QStringList>

class QTimer;

namespace Simulator
{
    class Connection;
    class ConnectionWorker;
}

class SensorGesturesConnection : public QObject
{
    Q_OBJECT
public:
    explicit SensorGesturesConnection(QObject *parent = 0);
    virtual ~SensorGesturesConnection();

Q_SIGNALS:
    void sensorGestureDetected();

public slots:
    void setSensorGestureData(const QString &);
    void newSensorGestureDetected();
    void newSensorGestures(const QStringList &gestures);

private:
    Simulator::Connection *mConnection;
    Simulator::ConnectionWorker *mWorker;
    QStringList allGestures;

};

QString get_qtSensorGestureData();

#endif //SIMULATORGESTURESCOMMON_H
