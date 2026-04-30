#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

#include "app_types.h"

namespace mycross {

    class CrosshairController : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool running READ running NOTIFY runningChanged)
        Q_PROPERTY(QString activeProfile READ activeProfile NOTIFY activeProfileChanged)
        Q_PROPERTY(QStringList profiles READ profileList NOTIFY profilesChanged)
        Q_PROPERTY(QVariantMap config READ config NOTIFY configChanged)
        Q_PROPERTY(QString hotkey READ hotkey CONSTANT)
        Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

    public:
        explicit CrosshairController(AppContext &app, QObject *parent = nullptr);

        bool running() const;
        QString activeProfile() const;
        QStringList profileList() const;
        QVariantMap config() const;
        QString hotkey() const;
        QString lastError() const;

        Q_INVOKABLE void refresh();
        Q_INVOKABLE bool setRunning(bool running);
        Q_INVOKABLE bool applyConfig(const QVariantMap &values);
        Q_INVOKABLE bool loadProfile(const QString &name);
        Q_INVOKABLE bool saveProfile(const QString &name, const QVariantMap &values);
        Q_INVOKABLE bool createProfile(const QString &name, const QVariantMap &values);
        Q_INVOKABLE bool renameProfile(const QString &oldName, const QString &newName);
        Q_INVOKABLE void quitApp();

    signals:
        void runningChanged();
        void activeProfileChanged();
        void profilesChanged();
        void configChanged();
        void lastErrorChanged();
        void quitRequested();

    private:
        AppContext &app_;
        mutable QTimer poll_timer_;
        bool running_cache_ = false;
        QString active_cache_;
        QStringList profiles_cache_;
        QVariantMap config_cache_;
        QString last_error_;

        void setLastError(const QString &message);
        void syncFromState();
        Config configFromMap(const QVariantMap &values, const Config &original) const;
    };

} // namespace mycross
