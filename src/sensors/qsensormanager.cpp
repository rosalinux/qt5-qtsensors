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

#include "qsensormanager.h"
#include <QDebug>
#include <private/qfactoryloader_p.h>
#include <QPluginLoader>
#include "qsensorplugin.h"
#include <QStandardPaths>
#include "sensorlog_p.h"
#include <QTimer>
#include <QFile>

QT_BEGIN_NAMESPACE

typedef QHash<QByteArray,QSensorBackendFactory*> FactoryForIdentifierMap;
typedef QHash<QByteArray,FactoryForIdentifierMap> BackendIdentifiersForTypeMap;

class QSensorManagerPrivate : public QObject
{
    friend class QSensorManager;

    Q_OBJECT
public:
    enum PluginLoadingState {
        NotLoaded,
        Loading,
        Loaded
    };
    QSensorManagerPrivate()
        : pluginLoadingState(NotLoaded)
        , loader(new QFactoryLoader("com.nokia.Qt.QSensorPluginInterface/1.0", QLatin1String("/sensors")))
        , defaultIdentifierForTypeLoaded(false)
        , sensorsChanged(false)
    {
    }

    PluginLoadingState pluginLoadingState;
    QFactoryLoader *loader;
    void loadPlugins();

    // Holds a mapping from type to available identifiers (and from there to the factory)
    BackendIdentifiersForTypeMap backendsByType;

    // Holds the default identifier
    QHash<QByteArray, QByteArray> defaultIdentifierForType;
    bool defaultIdentifierForTypeLoaded;

    // Holds the first identifier for each type
    QHash<QByteArray, QByteArray> firstIdentifierForType;

    bool sensorsChanged;
    QList<QSensorChangesInterface*> changeListeners;
    QSet <QObject *> seenPlugins;

Q_SIGNALS:
    void availableSensorsChanged();

public Q_SLOTS:
    void emitSensorsChanged()
    {
        static bool alreadyRunning = false;
        if (pluginLoadingState != QSensorManagerPrivate::Loaded || alreadyRunning) {
            // We're busy.
            // Just note that a registration changed and exit.
            // Someone up the call stack will deal with this.
            sensorsChanged = true;
            return;
        }

        // Set a flag so any recursive calls doesn't cause a loop.
        alreadyRunning = true;

        // Since one [un]registration may cause other [un]registrations and since
        // the order in which we do things matters we just do a cascading update
        // until things stop changing.
        do {
            sensorsChanged = false;
            Q_FOREACH (QSensorChangesInterface *changes, changeListeners) {
                changes->sensorsChanged();
            }
        } while (sensorsChanged);

        // We're going away now so clear the flag
        alreadyRunning = false;

        // Notify the client of the changes
        Q_EMIT availableSensorsChanged();
    }
};

Q_GLOBAL_STATIC(QSensorManagerPrivate, sensorManagerPrivate)

// The unit test needs to change the behaviour of the library. It does this
// through an exported but undocumented function.
static void initPlugin(QObject *plugin);
static bool load_external_plugins = true;
Q_SENSORS_EXPORT void sensors_unit_test_hook(int index)
{
    QSensorManagerPrivate *d = sensorManagerPrivate();

    switch (index) {
    case 0:
        Q_ASSERT(d->pluginLoadingState == QSensorManagerPrivate::NotLoaded);
        load_external_plugins = false;
        break;
    case 1:
    {
        Q_ASSERT(load_external_plugins == false);
        Q_ASSERT(d->pluginLoadingState == QSensorManagerPrivate::Loaded);
        SENSORLOG() << "initializing plugins";
        QList<QJsonObject> meta = d->loader->metaData();
        for (int i = 0; i < meta.count(); i++) {
            QObject *plugin = d->loader->instance(i);
            initPlugin(plugin);
        }
        break;
    }
    default:
        break;
    }
}

static void initPlugin(QObject *o)
{
    if (!o) return;

    QSensorManagerPrivate *d = sensorManagerPrivate();
    if (d->seenPlugins.contains(o))
        return;

    QSensorChangesInterface *changes = qobject_cast<QSensorChangesInterface*>(o);
    if (changes)
        d->changeListeners << changes;

    QSensorPluginInterface *plugin = qobject_cast<QSensorPluginInterface*>(o);

    if (plugin) {
        d->seenPlugins.insert(o);
        plugin->registerSensors();
    }
}

void QSensorManagerPrivate::loadPlugins()
{
    QSensorManagerPrivate *d = this;
    if (d->pluginLoadingState != QSensorManagerPrivate::NotLoaded) return;
    d->pluginLoadingState = QSensorManagerPrivate::Loading;

    SENSORLOG() << "initializing static plugins";
    // Qt-style static plugins
    Q_FOREACH (QObject *plugin, QPluginLoader::staticInstances()) {
        initPlugin(plugin);
    }

    if (load_external_plugins) {
        SENSORLOG() << "initializing plugins";
        QList<QJsonObject> meta = d->loader->metaData();
        for (int i = 0; i < meta.count(); i++) {
            QObject *plugin = d->loader->instance(i);
            initPlugin(plugin);
        }
    }

    d->pluginLoadingState = QSensorManagerPrivate::Loaded;

    if (d->sensorsChanged) {
        // Notify the app that the available sensor list has changed.
        // This may cause recursive calls!
        d->emitSensorsChanged();
    }
}

// =====================================================================

/*!
    \class QSensorManager
    \ingroup sensors_backend
    \inmodule QtSensors

    \brief The QSensorManager class handles registration and creation of sensor backends.

    Sensor plugins register backends using the registerBackend() function.

    When QSensor::connectToBackend() is called, the createBackend() function will be called.
*/

/*!
    Register a sensor for \a type. The \a identifier must be unique.

    The \a factory will be asked to create instances of the backend.
*/
void QSensorManager::registerBackend(const QByteArray &type, const QByteArray &identifier, QSensorBackendFactory *factory)
{
    Q_ASSERT(type.count());
    Q_ASSERT(identifier.count());
    Q_ASSERT(factory);
    QSensorManagerPrivate *d = sensorManagerPrivate();
    if (!d->backendsByType.contains(type)) {
        (void)d->backendsByType[type];
        d->firstIdentifierForType[type] = identifier;
    } else if (d->firstIdentifierForType[type].startsWith("generic.")) {
        // Don't let a generic backend be the default when some other backend exists!
        d->firstIdentifierForType[type] = identifier;
    }
    FactoryForIdentifierMap &factoryByIdentifier = d->backendsByType[type];
    if (factoryByIdentifier.contains(identifier)) {
        qWarning() << "A backend with type" << type << "and identifier" << identifier << "has already been registered!";
        return;
    }
    SENSORLOG() << "registering backend for type" << type << "identifier" << identifier;// << "factory" << QString().sprintf("0x%08x", (unsigned int)factory);
    factoryByIdentifier[identifier] = factory;

    // Notify the app that the available sensor list has changed.
    // This may cause recursive calls!
    d->emitSensorsChanged();
}

/*!
    Unregister the backend for \a type with \a identifier.

    Note that this only prevents new instance of the backend from being created. It does not
    invalidate the existing instances of the backend. The backend code should handle the
    disappearance of the underlying hardware itself.
*/
void QSensorManager::unregisterBackend(const QByteArray &type, const QByteArray &identifier)
{
    QSensorManagerPrivate *d = sensorManagerPrivate();
    if (!d->backendsByType.contains(type)) {
        qWarning() << "No backends of type" << type << "are registered";
        return;
    }
    FactoryForIdentifierMap &factoryByIdentifier = d->backendsByType[type];
    if (!factoryByIdentifier.contains(identifier)) {
        qWarning() << "Identifier" << identifier << "is not registered";
        return;
    }

    (void)factoryByIdentifier.take(identifier); // we don't own this pointer anyway
    if (d->firstIdentifierForType[type] == identifier) {
        if (factoryByIdentifier.count()) {
            d->firstIdentifierForType[type] = factoryByIdentifier.begin().key();
            if (d->firstIdentifierForType[type].startsWith("generic.")) {
                // Don't let a generic backend be the default when some other backend exists!
                for (FactoryForIdentifierMap::const_iterator it = factoryByIdentifier.begin()++; it != factoryByIdentifier.end(); it++) {
                    const QByteArray &identifier(it.key());
                    if (!identifier.startsWith("generic.")) {
                        d->firstIdentifierForType[type] = identifier;
                        break;
                    }
                }
            }
        } else {
            (void)d->firstIdentifierForType.take(type);
        }
    }
    if (!factoryByIdentifier.count())
        (void)d->backendsByType.take(type);

    // Notify the app that the available sensor list has changed.
    // This may cause recursive calls!
    d->emitSensorsChanged();
}

/*!
    Create a backend for \a sensor. Returns null if no suitable backend exists.
*/
QSensorBackend *QSensorManager::createBackend(QSensor *sensor)
{
    Q_ASSERT(sensor);

    QSensorManagerPrivate *d = sensorManagerPrivate();
    d->loadPlugins();

    SENSORLOG() << "QSensorManager::createBackend" << "type" << sensor->type() << "identifier" << sensor->identifier();

    if (!d->backendsByType.contains(sensor->type())) {
        SENSORLOG() << "no backends of type" << sensor->type() << "have been registered.";
        return 0;
    }

    const FactoryForIdentifierMap &factoryByIdentifier = d->backendsByType[sensor->type()];
    QSensorBackendFactory *factory;
    QSensorBackend *backend;

    if (sensor->identifier().isEmpty()) {
        QByteArray defaultIdentifier = QSensor::defaultSensorForType(sensor->type());
        SENSORLOG() << "Trying the default" << defaultIdentifier;
        // No identifier set, try the default
        factory = factoryByIdentifier[defaultIdentifier];
        //SENSORLOG() << "factory" << QString().sprintf("0x%08x", (unsigned int)factory);
        sensor->setIdentifier(defaultIdentifier); // the factory requires this
        backend = factory->createBackend(sensor);
        if (backend) return backend; // Got it!

        // The default failed to instantiate so try any other registered sensors for this type
        Q_FOREACH (const QByteArray &identifier, factoryByIdentifier.keys()) {
            SENSORLOG() << "Trying" << identifier;
            if (identifier == defaultIdentifier) continue; // Don't do the default one again
            factory = factoryByIdentifier[identifier];
            //SENSORLOG() << "factory" << QString().sprintf("0x%08x", (unsigned int)factory);
            sensor->setIdentifier(identifier); // the factory requires this
            backend = factory->createBackend(sensor);
            if (backend) return backend; // Got it!
        }
        SENSORLOG() << "FAILED";
        sensor->setIdentifier(QByteArray()); // clear the identifier
    } else {
        if (!factoryByIdentifier.contains(sensor->identifier())) {
            SENSORLOG() << "no backend with identifier" << sensor->identifier() << "for type" << sensor->type();
            return 0;
        }

        // We were given an explicit identifier so don't substitute other backends if it fails to instantiate
        factory = factoryByIdentifier[sensor->identifier()];
        //SENSORLOG() << "factory" << QString().sprintf("0x%08x", (unsigned int)factory);
        backend = factory->createBackend(sensor);
        if (backend) return backend; // Got it!
    }

    SENSORLOG() << "no suitable backend found for requested identifier" << sensor->identifier() << "and type" << sensor->type();
    return 0;
}

/*!
    Returns true if the backend identified by \a type and \a identifier is registered.

    This is a convenience method that helps out plugins doing dynamic registration.
*/
bool QSensorManager::isBackendRegistered(const QByteArray &type, const QByteArray &identifier)
{
    QSensorManagerPrivate *d = sensorManagerPrivate();
    d->loadPlugins();

    if (!d->backendsByType.contains(type))
        return false;

    const FactoryForIdentifierMap &factoryByIdentifier = d->backendsByType[type];
    if (!factoryByIdentifier.contains(identifier))
        return false;

    return true;
}

/*!
    Sets or overwrite the sensor \a type with the backend \a identifier.
*/
void QSensorManager::setDefaultBackend(const QByteArray &type, const QByteArray &identifier)
{
    QSensorManagerPrivate *d = sensorManagerPrivate();
    d->defaultIdentifierForType.insert(type, identifier);
}


// =====================================================================

/*!
    Returns a list of all sensor types.
*/
QList<QByteArray> QSensor::sensorTypes()
{
    QSensorManagerPrivate *d = sensorManagerPrivate();
    d->loadPlugins();

    return d->backendsByType.keys();
}

/*!
    Returns a list of ids for each of the sensors for \a type.
    If there are no sensors of that type available the list will be empty.
*/
QList<QByteArray> QSensor::sensorsForType(const QByteArray &type)
{
    QSensorManagerPrivate *d = sensorManagerPrivate();
    d->loadPlugins();

    // no sensors of that type exist
    if (!d->backendsByType.contains(type))
        return QList<QByteArray>();

    return d->backendsByType[type].keys();
}

/*!
    Returns the default sensor identifier for \a type.
    This is set in a config file and can be overridden if required.
    If no default is available the system will return the first registered
    sensor for \a type.

    Note that there is special case logic to prevent the generic plugin's backends from becoming the
    default when another backend is registered for the same type. This logic means that a backend
    identifier starting with \c{generic.} will only be the default if no other backends have been
    registered for that type or if it is specified in \c{Sensors.conf}.

    \sa {Determining the default sensor for a type}
*/
QByteArray QSensor::defaultSensorForType(const QByteArray &type)
{
    QSensorManagerPrivate *d = sensorManagerPrivate();
    d->loadPlugins();

    // no sensors of that type exist
    if (!d->backendsByType.contains(type))
        return QByteArray();

    //check if we need to read the config setting file
    if (!d->defaultIdentifierForTypeLoaded) {
        d->defaultIdentifierForTypeLoaded = true;
        QStringList abspath = QStandardPaths::standardLocations(QStandardPaths::ConfigLocation);
        QString cfgpath = "";
        //first in the list is user specific, so ignore it and take the last
        if (abspath.length() > 0) {
            cfgpath = abspath[abspath.count() - 1];
            cfgpath += QString::fromLatin1("/Nokia/Sensors.conf");
            if (QFile::exists(cfgpath)){
                QFile cfgfile(cfgpath);
                if (cfgfile.open(QFile::ReadOnly)){
                    //Read the sensor default setting file
                    QTextStream stream(&cfgfile);
                    QString line = "";
                    bool isconfig = false;
                    while (!stream.atEnd()) {
                        line = stream.readLine();
                        if (!isconfig && line == QString::fromLatin1("[Default]"))
                            isconfig = true;
                        else {
                            if (isconfig) {
                                //read out setting line
                                line.remove(' ');
                                QStringList pair = line.split('=');
                                if (pair.count() == 2)
                                    d->defaultIdentifierForType.insert(pair[0].toLatin1(), pair[1].toLatin1());
                            }
                        }
                    }
                }
            }
        }
    }

    QHash<QByteArray, QByteArray>::const_iterator i = d->defaultIdentifierForType.find(type);
    if (i != d->defaultIdentifierForType.end() && i.key() == type) {
        if (d->backendsByType[type].contains(i.value())) // Don't return a value that we can't use!
            return i.value();
    }

    // This is our fallback
    return d->firstIdentifierForType[type];
}

void QSensor::registerInstance()
{
    QSensorManagerPrivate *d = sensorManagerPrivate();
    connect(d, SIGNAL(availableSensorsChanged()), this, SIGNAL(availableSensorsChanged()));
}

// =====================================================================

/*!
    \class QSensorBackendFactory
    \ingroup sensors_backend
    \inmodule QtSensors

    \brief The QSensorBackendFactory class instantiates instances of
           QSensorBackend.

    This interface must be implemented in order to register a sensor backend.

    \sa {Creating a sensor plugin}
*/

/*!
    \fn QSensorBackendFactory::~QSensorBackendFactory()
    \internal
*/

/*!
    \fn QSensorBackendFactory::createBackend(QSensor *sensor)

    Instantiate a backend. If the factory handles multiple identifiers
    it should check with the \a sensor to see which one is requested.

    If the factory cannot create a backend it should return 0.
*/

QT_END_NAMESPACE

#include "qsensormanager.moc"
