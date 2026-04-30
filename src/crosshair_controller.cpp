#include "crosshair_controller.h"

#include <QCoreApplication>
#include <QVariant>

#include "common.h"
#include "config_store.h"
#include "overlay.h"

namespace mycross {
    namespace {

        QString qstr(const std::wstring &value) {
            return QString::fromStdWString(value);
        }

        std::wstring wstr(const QString &value) {
            return value.toStdWString();
        }

        QVariantMap cfg_map(const Config &cfg) {
            return {
                {"x", cfg.x},
                {"y", cfg.y},
                {"window_size", cfg.window_size},
                {"cross_half", cfg.cross_half},
                {"line_width", cfg.line_width},
                {"color_r", cfg.color_r},
                {"color_g", cfg.color_g},
                {"color_b", cfg.color_b},
            };
        }

        QStringList q_profiles(const std::vector<std::wstring> &items) {
            QStringList out;
            for (const auto &item : items) {
                out.push_back(qstr(item));
            }
            return out;
        }

        int map_int(const QVariantMap &values, const char *key, int fallback) {
            const auto it = values.find(QString::fromLatin1(key));
            if (it == values.end()) {
                return fallback;
            }
            bool ok = false;
            const int value = it.value().toInt(&ok);
            return ok ? value : fallback;
        }

    } // namespace

    CrosshairController::CrosshairController(AppContext &app, QObject *parent)
        : QObject(parent), app_(app) {
        syncFromState();
        poll_timer_.setInterval(200);
        connect(&poll_timer_, &QTimer::timeout, this, &CrosshairController::syncFromState);
        poll_timer_.start();
    }

    bool CrosshairController::running() const {
        return running_cache_;
    }

    QString CrosshairController::activeProfile() const {
        return active_cache_;
    }

    QStringList CrosshairController::profileList() const {
        return profiles_cache_;
    }

    QVariantMap CrosshairController::config() const {
        return config_cache_;
    }

    QString CrosshairController::hotkey() const {
        return QStringLiteral("Ctrl + Alt + Shift + F12");
    }

    QString CrosshairController::lastError() const {
        return last_error_;
    }

    void CrosshairController::refresh() {
        syncFromState();
    }

    bool CrosshairController::setRunning(bool running) {
        {
            std::lock_guard<std::mutex> lock(app_.state.mu);
            app_.state.running = running;
        }
        post_sync(app_);
        syncFromState();
        return true;
    }

    bool CrosshairController::applyConfig(const QVariantMap &values) {
        {
            std::lock_guard<std::mutex> lock(app_.state.mu);
            app_.state.cfg = configFromMap(values, app_.state.cfg);
        }
        post_sync(app_);
        syncFromState();
        return true;
    }

    bool CrosshairController::loadProfile(const QString &name) {
        const auto clean_name = profile_name(wstr(name));
        if (clean_name.empty()) {
            setLastError(tr("配置名无效"));
            return false;
        }

        const auto file = profile_path(app_, clean_name);
        if (GetFileAttributesW(file.c_str()) == INVALID_FILE_ATTRIBUTES) {
            setLastError(tr("配置不存在"));
            return false;
        }

        const Config loaded = load_cfg(file);
        {
            std::lock_guard<std::mutex> lock(app_.state.mu);
            app_.state.cfg = loaded;
            app_.state.active = clean_name;
        }
        post_sync(app_);
        syncFromState();
        return true;
    }

    bool CrosshairController::saveProfile(const QString &name, const QVariantMap &values) {
        std::wstring clean_name;
        {
            std::lock_guard<std::mutex> lock(app_.state.mu);
            clean_name = app_.state.active;
        }
        const auto requested = profile_name(wstr(name));
        if (!requested.empty()) {
            clean_name = requested;
        }
        if (clean_name.empty()) {
            setLastError(tr("配置名无效"));
            return false;
        }

        Config out;
        {
            std::lock_guard<std::mutex> lock(app_.state.mu);
            app_.state.cfg = configFromMap(values, app_.state.cfg);
            app_.state.active = clean_name;
            out = app_.state.cfg;
        }
        if (!save_cfg(profile_path(app_, clean_name), out)) {
            setLastError(tr("保存配置失败"));
            return false;
        }
        post_sync(app_);
        syncFromState();
        return true;
    }

    bool CrosshairController::createProfile(const QString &name, const QVariantMap &values) {
        const auto clean_name = profile_name(wstr(name));
        if (clean_name.empty()) {
            setLastError(tr("配置名无效"));
            return false;
        }

        const auto file = profile_path(app_, clean_name);
        if (GetFileAttributesW(file.c_str()) != INVALID_FILE_ATTRIBUTES) {
            setLastError(tr("配置已存在"));
            return false;
        }

        Config out;
        {
            std::lock_guard<std::mutex> lock(app_.state.mu);
            app_.state.cfg = configFromMap(values, app_.state.cfg);
            app_.state.active = clean_name;
            out = app_.state.cfg;
        }
        if (!save_cfg(file, out)) {
            setLastError(tr("新建配置失败"));
            return false;
        }
        post_sync(app_);
        syncFromState();
        return true;
    }

    bool CrosshairController::renameProfile(const QString &oldName, const QString &newName) {
        std::wstring clean_old = profile_name(wstr(oldName));
        if (clean_old.empty()) {
            std::lock_guard<std::mutex> lock(app_.state.mu);
            clean_old = app_.state.active;
        }
        const auto clean_new = profile_name(wstr(newName));
        if (clean_old.empty() || clean_new.empty()) {
            setLastError(tr("配置名无效"));
            return false;
        }
        if (_wcsicmp(clean_old.c_str(), clean_new.c_str()) == 0) {
            syncFromState();
            return true;
        }

        const auto old_file = profile_path(app_, clean_old);
        const auto new_file = profile_path(app_, clean_new);
        if (GetFileAttributesW(old_file.c_str()) == INVALID_FILE_ATTRIBUTES) {
            setLastError(tr("原配置不存在"));
            return false;
        }
        if (GetFileAttributesW(new_file.c_str()) != INVALID_FILE_ATTRIBUTES) {
            setLastError(tr("目标配置已存在"));
            return false;
        }
        if (!MoveFileW(old_file.c_str(), new_file.c_str())) {
            setLastError(tr("重命名配置失败"));
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(app_.state.mu);
            if (_wcsicmp(app_.state.active.c_str(), clean_old.c_str()) == 0) {
                app_.state.active = clean_new;
            }
        }
        syncFromState();
        return true;
    }

    void CrosshairController::quitApp() {
        app_.exit = true;
        emit quitRequested();
        QCoreApplication::quit();
    }

    void CrosshairController::setLastError(const QString &message) {
        if (last_error_ == message) {
            return;
        }
        last_error_ = message;
        emit lastErrorChanged();
    }

    void CrosshairController::syncFromState() {
        Config cfg;
        bool running = false;
        std::wstring active;
        {
            std::lock_guard<std::mutex> lock(app_.state.mu);
            cfg = app_.state.cfg;
            running = app_.state.running;
            active = app_.state.active;
        }

        const QVariantMap next_config = cfg_map(cfg);
        const QString next_active = qstr(active);
        const QStringList next_profiles = q_profiles(profiles(app_));

        if (running_cache_ != running) {
            running_cache_ = running;
            emit runningChanged();
        }
        if (active_cache_ != next_active) {
            active_cache_ = next_active;
            emit activeProfileChanged();
        }
        if (profiles_cache_ != next_profiles) {
            profiles_cache_ = next_profiles;
            emit profilesChanged();
        }
        if (config_cache_ != next_config) {
            config_cache_ = next_config;
            emit configChanged();
        }
        if (!last_error_.isEmpty()) {
            last_error_.clear();
            emit lastErrorChanged();
        }
    }

    Config CrosshairController::configFromMap(const QVariantMap &values,
                                              const Config &original) const {
        Config cfg = original;
        cfg.x = map_int(values, "x", cfg.x);
        cfg.y = map_int(values, "y", cfg.y);
        cfg.window_size = map_int(values, "window_size", cfg.window_size);
        cfg.cross_half = map_int(values, "cross_half", cfg.cross_half);
        cfg.line_width = map_int(values, "line_width", cfg.line_width);
        cfg.color_r = map_int(values, "color_r", cfg.color_r);
        cfg.color_g = map_int(values, "color_g", cfg.color_g);
        cfg.color_b = map_int(values, "color_b", cfg.color_b);
        normalize(cfg);
        return cfg;
    }

} // namespace mycross
