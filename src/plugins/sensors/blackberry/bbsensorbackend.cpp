/****************************************************************************
**
** Copyright (C) 2012 Research In Motion
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
#include "bbsensorbackend.h"

#include "bbguihelper.h"
#include <QtCore/QDebug>
#include <QtCore/qmath.h>
#include <fcntl.h>

static const int microSecondsPerSecond = 1000 * 1000;
static const int defaultBufferSize = 10;

static int microSecondsToHertz(uint microSeconds)
{
    return microSecondsPerSecond / microSeconds;
}

static uint hertzToMicroSeconds(int hertz)
{
    return microSecondsPerSecond / hertz;
}

static void remapMatrix(const float inputMatrix[3*3],
                        const float mappingMatrix[4],
                        float outputMatrix[3*3])
{
    int i,j,k;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 2; j++) { //only goto 2 because last column stays unchanged

            outputMatrix[i*3+j] = 0;

            for (k = 0; k < 2; k++) { //only goto 2 because we know rotation matrix is zero in bottom row
                outputMatrix[i*3+j] += inputMatrix[i*3+k] * mappingMatrix[k*2+j];
            }
        }

        outputMatrix[i*3+2] = inputMatrix[i*3+2];
    }
}

BbSensorBackendBase::BbSensorBackendBase(const QString &devicePath, sensor_type_e sensorType,
                                         QSensor *sensor)
    : QSensorBackend(sensor), m_deviceFile(devicePath), m_sensorType(sensorType), m_guiHelper(0)
{
    m_mappingMatrix[0] = m_mappingMatrix[3] = 1;
    m_mappingMatrix[1] = m_mappingMatrix[2] = 0;
    connect(sensor, SIGNAL(alwaysOnChanged()), this, SLOT(applyAlwaysOnProperty()));

    // Set some sensible default values
    sensor->setProperty("efficientBufferSize", defaultBufferSize);
    sensor->setProperty("maxBufferSize", defaultBufferSize);
}

BbGuiHelper *BbSensorBackendBase::guiHelper() const
{
    return m_guiHelper;
}

QFile &BbSensorBackendBase::deviceFile()
{
    return m_deviceFile;
}

sensor_type_e BbSensorBackendBase::sensorType() const
{
    return m_sensorType;
}

void BbSensorBackendBase::initSensorInfo()
{
    if (!m_deviceFile.open(QFile::ReadOnly)) {
        qDebug() << "initSensorInfo(): Failed to open" << m_deviceFile.fileName()
                 << ":" << m_deviceFile.errorString();
    } else {
        sensor_devctl_info_u deviceInfo;
        const int result = devctl(m_deviceFile.handle(), DCMD_SENSOR_INFO, &deviceInfo,
                                  sizeof(deviceInfo), NULL);
        if (result != EOK) {
            perror(QString::fromLatin1("Querying sensor info for %1 failed")
                        .arg(m_deviceFile.fileName()).toLocal8Bit());
        } else {
            if (addDefaultRange()) {
                addOutputRange(convertValue(deviceInfo.rx.info.range_min),
                               convertValue(deviceInfo.rx.info.range_max),
                               convertValue(deviceInfo.rx.info.resolution));
            }

            // Min and max intentionally swapped here, as the minimum delay is the maximum rate
            addDataRate(microSecondsToHertz(deviceInfo.rx.info.delay_max),
                        microSecondsToHertz(deviceInfo.rx.info.delay_min));
        }
        additionalDeviceInit();
        m_deviceFile.close();
    }
}

void BbSensorBackendBase::setGuiHelper(BbGuiHelper *guiHelper)
{
    Q_ASSERT(!m_guiHelper);
    m_guiHelper = guiHelper;
    connect(m_guiHelper, SIGNAL(applicationActiveChanged()), this, SLOT(updatePauseState()));
    connect(guiHelper, SIGNAL(orientationChanged()), this, SLOT(updateOrientation()));
    updateOrientation();
}

void BbSensorBackendBase::additionalDeviceInit()
{
}

bool BbSensorBackendBase::addDefaultRange()
{
    return true;
}

qreal BbSensorBackendBase::convertValue(float bbValue)
{
    return bbValue;
}

bool BbSensorBackendBase::isAutoAxisRemappingEnabled() const
{
    return sensor()->property("automaticAxisRemapping").toBool();
}

void BbSensorBackendBase::remapMatrix(const float inputMatrix[], float outputMatrix[])
{
    if (!isAutoAxisRemappingEnabled() || m_guiHelper->currentOrientation() == 0) {
        memcpy(outputMatrix, inputMatrix, sizeof(float) * 9);
        return;
    }

    ::remapMatrix(inputMatrix, m_mappingMatrix, outputMatrix);
}

void BbSensorBackendBase::remapAxes(float *x, float *y, float *z)
{
    Q_ASSERT(x && y && z);
    if (!isAutoAxisRemappingEnabled() || m_guiHelper->currentOrientation() == 0)
        return;

    const int angle = m_guiHelper->currentOrientation();

    const float oldX = *x;
    const float oldY = *y;

    switch (angle) {
    case 90:
        *x = -oldY;
        *y = oldX;
    break;
    case 180:
        *x = -oldX;
        *y = -oldY;
    break;
    case 270:
        *x = oldY;
        *y = -oldX;
    break;
    }
}

void BbSensorBackendBase::start()
{
    Q_ASSERT(m_guiHelper);

    if (!m_deviceFile.open(QFile::ReadOnly | QFile::Unbuffered)) {
        qDebug() << "Starting sensor" << m_deviceFile.fileName()
                 << "failed:" << m_deviceFile.errorString();
        sensorError(m_deviceFile.error());
        return;
    }

    const int rateInHertz = sensor()->dataRate();
    if (rateInHertz != 0) {
        const uint rateInMicroseconds = hertzToMicroSeconds(rateInHertz);
        sensor_devctl_rate_u deviceRate;
        deviceRate.tx.rate = rateInMicroseconds;
        const int result = devctl(m_deviceFile.handle(), DCMD_SENSOR_RATE, &deviceRate,
                                  sizeof(deviceRate), NULL);
        if (result != EOK) {
            perror(QString::fromLatin1("Setting sensor rate for %1 failed")
                   .arg(m_deviceFile.fileName()).toLocal8Bit());
        }
    }

    // Explicitly switch to non-blocking mode, otherwise read() will wait until new sensor
    // data is available, and we have no way to check if there is more data or not (bytesAvailable()
    // does not work for unbuffered mode)
    const int oldFlags = fcntl(m_deviceFile.handle(), F_GETFL);
    if (fcntl(m_deviceFile.handle(), F_SETFL, oldFlags | O_NONBLOCK) == -1) {
        perror(QString::fromLatin1("Starting sensor %1 failed, fcntl() returned -1")
                    .arg(m_deviceFile.fileName()).toLocal8Bit());
        sensorError(errno);
        stop();
        return;
    }

    // Activate event queuing if needed
    bool ok = false;
    const int requestedBufferSize = sensor()->property("bufferSize").toInt(&ok);
    if (ok && requestedBufferSize > 1) {
        sensor_devctl_queue_u queueControl;
        queueControl.tx.enable = 1;
        const int result = devctl(m_deviceFile.handle(), DCMD_SENSOR_QUEUE, &queueControl, sizeof(queueControl), NULL);
        if (result != EOK) {
            perror(QString::fromLatin1("Enabling sensor queuing for %1 failed")
                   .arg(m_deviceFile.fileName()).toLocal8Bit());
        }

        const int actualBufferSize = queueControl.rx.size;
        sensor()->setProperty("bufferSize", actualBufferSize);
        sensor()->setProperty("efficientBufferSize", actualBufferSize);
        sensor()->setProperty("maxBufferSize", actualBufferSize);
    }

    applyAlwaysOnProperty();

    m_socketNotifier.reset(new QSocketNotifier(m_deviceFile.handle(), QSocketNotifier::Read));
    connect(m_socketNotifier.data(), SIGNAL(activated(int)), this, SLOT(dataAvailable()));
}

void BbSensorBackendBase::stop()
{
    m_socketNotifier.reset();
    m_deviceFile.close();
}

bool BbSensorBackendBase::isFeatureSupported(QSensor::Feature feature) const
{
    switch (feature) {
    case QSensor:: AlwaysOn:
        return true;
    case QSensor::Buffering:
    case QSensor::GeoValues:
    case QSensor::FieldOfView:
        break;
    }

    return false;
}

void BbSensorBackendBase::dataAvailable()
{
    Q_FOREVER {
        sensor_event_t event;
        const qint64 numBytes = m_deviceFile.read(reinterpret_cast<char *>(&event),
                                                  sizeof(sensor_event_t));
        if (numBytes == -1) {
            break;
        } else if (numBytes == sizeof(sensor_event_t)) {
            processEvent(event);
        } else {
            qDebug() << "Reading sensor event data for" << m_deviceFile.fileName()
                     << "failed (unexpected data size):" << m_deviceFile.errorString();
        }
    }
}

void BbSensorBackendBase::applyAlwaysOnProperty()
{
    if (!m_deviceFile.isOpen())
        return;

    sensor_devctl_bkgrnd_u bgState;
    bgState.tx.enable = sensor()->isAlwaysOn() ? 1 : 0;

    const int result = devctl(m_deviceFile.handle(), DCMD_SENSOR_BKGRND, &bgState, sizeof(bgState), NULL);
    if (result != EOK) {
        perror(QString::fromLatin1("Setting sensor always on for %1 failed")
               .arg(m_deviceFile.fileName()).toLocal8Bit());
    }

    // We might need to pause now
    updatePauseState();
}

void BbSensorBackendBase::setPaused(bool paused)
{
    if (!m_deviceFile.isOpen())
        return;

    sensor_devctl_enable_u enableState;
    enableState.tx.enable = paused ? 0 : 1;

    const int result = devctl(m_deviceFile.handle(), DCMD_SENSOR_ENABLE, &enableState, sizeof(enableState), NULL);
    if (result != EOK) {
        perror(QString::fromLatin1("Setting sensor enabled (%1) for %2 failed")
               .arg(paused)
               .arg(m_deviceFile.fileName()).toLocal8Bit());
    }
}

void BbSensorBackendBase::updatePauseState()
{
    setPaused(!sensor()->isAlwaysOn() && !m_guiHelper->applicationActive());
}

void BbSensorBackendBase::updateOrientation()
{
    // ### I can't really test this, the rotation matrix has too many glitches and drifts over time,
    // making any measurement quite hard
    const int rotationAngle = guiHelper()->currentOrientation();

    m_mappingMatrix[0] = cos(rotationAngle*M_PI/180);
    m_mappingMatrix[1] = sin(rotationAngle*M_PI/180);
    m_mappingMatrix[2] = -sin(rotationAngle*M_PI/180);
    m_mappingMatrix[3] = cos(rotationAngle*M_PI/180);
}
