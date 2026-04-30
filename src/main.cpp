#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStringList>

#include "app_types.h"
#include "common.h"
#include "config_store.h"
#include "crosshair_controller.h"
#include "overlay.h"

namespace {

    void apply_cli(mycross::AppContext &app, const QStringList &args) {
        mycross::Config cfg;
        {
            std::lock_guard<std::mutex> lock(app.state.mu);
            cfg = app.state.cfg;
        }

        if (args.size() >= 3) {
            cfg.x = args.at(1).toInt();
            cfg.y = args.at(2).toInt();
        }
        for (int i = 1; i < args.size(); ++i) {
            const QString &arg = args.at(i);
            if (arg.startsWith(QStringLiteral("--x="))) {
                cfg.x = arg.mid(4).toInt();
            } else if (arg.startsWith(QStringLiteral("--y="))) {
                cfg.y = arg.mid(4).toInt();
            }
        }

        mycross::normalize(cfg);
        {
            std::lock_guard<std::mutex> lock(app.state.mu);
            app.state.cfg = cfg;
        }
    }

} // namespace

int main(int argc, char *argv[]) {
    QGuiApplication qt_app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("MyCross"));
    QGuiApplication::setOrganizationName(QStringLiteral("MyCross"));

    mycross::AppContext app;
    app.inst = GetModuleHandleW(nullptr);
    app.exe_dir = mycross::exe_dir();
    app.cfg_dir = app.exe_dir + L"\\configs";

    mycross::ensure_cfg(app);
    {
        std::lock_guard<std::mutex> lock(app.state.mu);
        app.state.active = L"default.ini";
        app.state.cfg = mycross::load_cfg(mycross::profile_path(app, app.state.active));
        app.state.running = false;
    }

    apply_cli(app, QCoreApplication::arguments());
    mycross::start_overlay(app);
    mycross::post_sync(app);

    mycross::CrosshairController controller(app);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("crosshair"), &controller);

    QObject::connect(&controller, &mycross::CrosshairController::quitRequested, &qt_app,
                     &QCoreApplication::quit);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &qt_app,
                     []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);

    engine.loadFromModule(QStringLiteral("MyCross"), QStringLiteral("Main"));
    const int rc = qt_app.exec();

    app.exit = true;
    mycross::stop_overlay(app);
    return rc;
}
