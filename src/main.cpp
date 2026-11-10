// blackout — OLED screen curtain, LayerOverlay key-combo lock, and OLED care utility.

#include <QtGlobal>

#if defined(_WIN32) && !defined(Q_OS_WIN)
#define Q_OS_WIN
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <LayerShellQt/Window>
#endif
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <csignal>

#include "backend.h"
#include "theme.h"
#include "translator.h"

namespace {
void handleUnixSignal(int) {
    QCoreApplication::quit();
}

bool isHeadlessCommand(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromUtf8(argv[i]);
        if (arg.startsWith('-'))
            continue;
        return arg == "status" || arg == "check" || arg == "set" || arg == "skill";
    }
    return false;
}

bool parseBoolFlag(const QString &value, bool *okOut) {
    const QString v = value.trimmed().toLower();
    if (v == "on" || v == "true" || v == "1" || v == "yes") {
        *okOut = true;
        return true;
    }
    if (v == "off" || v == "false" || v == "0" || v == "no") {
        *okOut = true;
        return false;
    }
    *okOut = false;
    return false;
}

int runHeadlessCommand(const QString &cmd, const QStringList &positional, const QCommandLineParser &parser,
                       const QCommandLineOption &jsonOpt, const QCommandLineOption &unlockOpt,
                       const QCommandLineOption &autoOpt, const QCommandLineOption &minutesOpt,
                       const QCommandLineOption &zeroBrightOpt, const QCommandLineOption &silenceOpt,
                       const QCommandLineOption &indicatorOpt, const QCommandLineOption &brightnessOpt) {
    QTextStream out(stdout);
    QTextStream err(stderr);
    const bool json = parser.isSet(jsonOpt);

    if (cmd == "skill") {
        if (positional.size() >= 2 && positional.at(1) == "install") {
            QString errorMsg;
            if (!Backend::installSkill({}, &errorMsg)) {
                err << errorMsg << "\n";
                return 1;
            }
            if (json) {
                QJsonObject res{{"installed", true}};
                out << QString::fromUtf8(QJsonDocument(res).toJson(QJsonDocument::Compact)) << "\n";
            } else {
                out << "Installed blackout skill to ~/.agents/skills/blackout/SKILL.md\n";
            }
            return 0;
        }
        if (json) {
            QJsonObject res{{"skill", Backend::skillMarkdown()}};
            out << QString::fromUtf8(QJsonDocument(res).toJson(QJsonDocument::Compact)) << "\n";
        } else {
            out << Backend::skillMarkdown();
        }
        return 0;
    }

    Backend backend;

    if (cmd == "check") {
        const QStringList problems = backend.checkProblems();
        if (json) {
            QJsonObject res = backend.toJson();
            res.insert("ok", problems.isEmpty());
            out << QString::fromUtf8(QJsonDocument(res).toJson(QJsonDocument::Compact)) << "\n";
        } else if (problems.isEmpty()) {
            out << "ok\n";
        }
        for (const QString &p : problems)
            err << p << "\n";
        return problems.isEmpty() ? 0 : 1;
    }

    if (cmd == "set") {
        if (parser.isSet(unlockOpt)) {
            if (!backend.setUnlockSequence(parser.value(unlockOpt))) {
                err << "Invalid unlock key combo (must include a modifier such as Ctrl+Shift+U).\n";
                return 1;
            }
        }
        if (parser.isSet(autoOpt)) {
            bool ok = false;
            const bool val = parseBoolFlag(parser.value(autoOpt), &ok);
            if (!ok) {
                err << "Invalid value for --auto (expected on or off).\n";
                return 1;
            }
            backend.setAutoBlackout(val);
        }
        if (parser.isSet(minutesOpt)) {
            bool ok = false;
            const int mins = parser.value(minutesOpt).toInt(&ok);
            if (!ok || mins < 1 || mins > 120) {
                err << "Invalid value for --minutes (expected 1..120).\n";
                return 1;
            }
            backend.setAutoBlackoutMinutes(mins);
        }
        if (parser.isSet(zeroBrightOpt)) {
            bool ok = false;
            const bool val = parseBoolFlag(parser.value(zeroBrightOpt), &ok);
            if (!ok) {
                err << "Invalid value for --zero-brightness (expected on or off).\n";
                return 1;
            }
            backend.setZeroBrightnessOnBlackout(val);
        }
        if (parser.isSet(silenceOpt)) {
            bool ok = false;
            const bool val = parseBoolFlag(parser.value(silenceOpt), &ok);
            if (!ok) {
                err << "Invalid value for --silence-notifications (expected on or off).\n";
                return 1;
            }
            backend.setSilenceNotificationsOnBlackout(val);
        }
        if (parser.isSet(indicatorOpt)) {
            bool ok = false;
            const bool val = parseBoolFlag(parser.value(indicatorOpt), &ok);
            if (!ok) {
                err << "Invalid value for --indicator (expected on or off).\n";
                return 1;
            }
            backend.setShowStatusIndicator(val);
        }
        if (parser.isSet(brightnessOpt)) {
            bool ok = false;
            const int pct = parser.value(brightnessOpt).toInt(&ok);
            if (!ok || pct < 5 || pct > 100) {
                err << "Invalid value for --brightness (expected 5..100).\n";
                return 1;
            }
            backend.setBrightnessPreset(pct);
        }
    }

    if (json) {
        out << QString::fromUtf8(QJsonDocument(backend.toJson()).toJson(QJsonDocument::Compact)) << "\n";
    } else {
        out << "Unlock shortcut: " << backend.unlockSequence() << "\n"
            << "Auto-blackout: " << (backend.autoBlackout() ? "on" : "off")
            << " (" << backend.autoBlackoutMinutes() << " min)\n"
            << "0% hardware backlight: " << (backend.zeroBrightnessOnBlackout() ? "on" : "off") << "\n"
            << "Silence notifications: " << (backend.silenceNotificationsOnBlackout() ? "on" : "off") << "\n"
            << "Brightness: " << backend.brightnessPercent() << "%\n"
            << "Night Light: " << (backend.nightLightEnabled() ? "on" : "off") << "\n";
    }
    return 0;
}

void configureLayerOverlayWindow(QQuickWindow *curtainWindow) {
    if (!curtainWindow)
        return;
#ifdef Q_OS_WIN
    curtainWindow->setFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    if (auto *screen = QGuiApplication::primaryScreen()) {
        curtainWindow->setGeometry(screen->virtualGeometry());
    }
#else
    if (!QGuiApplication::platformName().contains("wayland", Qt::CaseInsensitive))
        return;

    if (auto *layerWin = LayerShellQt::Window::get(curtainWindow)) {
        layerWin->setScope("blackout-curtain");
        layerWin->setLayer(LayerShellQt::Window::LayerOverlay);
        layerWin->setAnchors(LayerShellQt::Window::Anchors(
            LayerShellQt::Window::AnchorTop |
            LayerShellQt::Window::AnchorBottom |
            LayerShellQt::Window::AnchorLeft |
            LayerShellQt::Window::AnchorRight));
        layerWin->setExclusiveZone(-1);
        layerWin->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
        layerWin->setWantsToBeOnActiveScreen(true);
        layerWin->setActivateOnShow(true);
    }
#endif
}
} // namespace

int main(int argc, char *argv[]) {
#ifdef Q_OS_WIN
    SetConsoleCtrlHandler([](DWORD) -> BOOL {
        QCoreApplication::quit();
        return TRUE;
    }, TRUE);
    std::signal(SIGINT, handleUnixSignal);
    std::signal(SIGTERM, handleUnixSignal);
#else
    std::signal(SIGINT, handleUnixSignal);
    std::signal(SIGTERM, handleUnixSignal);
    std::signal(SIGHUP, handleUnixSignal);
#endif

    // Headless commands run without a display server.
    if (isHeadlessCommand(argc, argv)) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName("blackout");
    app.setApplicationVersion("0.2.1");

    // Matches the window to blackout.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("blackout");
    app.setWindowIcon(QIcon::fromTheme("blackout"));

    QCommandLineParser parser;
    parser.setApplicationDescription("OLED screen curtain, LayerOverlay key-combo lock, and OLED care utility");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("command", "open | lock | status | check | set | skill [install]");

    QCommandLineOption nowOption(QStringList{"n", "now"}, "Start immediately in fullscreen OLED blackout mode.");
    QCommandLineOption quitOnUnlockOption(QStringList{"q", "quit-on-unlock"}, "Quit as soon as the unlock key combo is pressed.");
    QCommandLineOption jsonOption("json", "Format CLI output as JSON.");
    QCommandLineOption unlockOption("unlock", "Set unlock key combo (e.g. Ctrl+Shift+U).", "combo");
    QCommandLineOption autoOption("auto", "Enable or disable auto-blackout timer (on|off).", "state");
    QCommandLineOption minutesOption("minutes", "Set auto-blackout inactivity minutes (1..120).", "minutes");
    QCommandLineOption zeroBrightOption("zero-brightness", "Cut hardware backlight to 0% when locked (on|off).", "state");
    QCommandLineOption silenceOption("silence-notifications", "Silence notifications when locked (on|off).", "state");
    QCommandLineOption indicatorOption("indicator", "Show pixel-shifted status badge when locked (on|off).", "state");
    QCommandLineOption brightnessOption("brightness", "Set OLED hardware brightness preset (5..100).", "percent");

    parser.addOption(nowOption);
    parser.addOption(quitOnUnlockOption);
    parser.addOption(jsonOption);
    parser.addOption(unlockOption);
    parser.addOption(autoOption);
    parser.addOption(minutesOption);
    parser.addOption(zeroBrightOption);
    parser.addOption(silenceOption);
    parser.addOption(indicatorOption);
    parser.addOption(brightnessOption);
    parser.process(app);

    const QStringList positional = parser.positionalArguments();
    const QString command = positional.isEmpty() ? QStringLiteral("open") : positional.first();

    if (command == "status" || command == "check" || command == "set" || command == "skill") {
        return runHeadlessCommand(command, positional, parser, jsonOption, unlockOption, autoOption,
                                  minutesOption, zeroBrightOption, silenceOption, indicatorOption, brightnessOption);
    }

    const bool startNow = parser.isSet(nowOption) || command == "lock";
    const bool quitOnUnlock = parser.isSet(quitOnUnlockOption);

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Backend backend;

    auto updateCursor = [&backend]() {
        if (backend.active() || backend.pixelRefresherActive()) {
            if (QGuiApplication::overrideCursor() == nullptr)
                QGuiApplication::setOverrideCursor(Qt::BlankCursor);
        } else {
            while (QGuiApplication::overrideCursor() != nullptr)
                QGuiApplication::restoreOverrideCursor();
        }
    };
    QObject::connect(&backend, &Backend::activeChanged, &app, updateCursor);
    QObject::connect(&backend, &Backend::pixelRefresherChanged, &app, updateCursor);

    if (quitOnUnlock) {
        QObject::connect(&backend, &Backend::unlocked, &app, &QCoreApplication::quit);
    }

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    auto *mainWindow = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    auto *curtainWindow = mainWindow ? mainWindow->findChild<QQuickWindow *>("curtainWindow") : nullptr;

    // Attach wlr-layer-shell LayerOverlay + ExclusiveZone(-1) + Exclusive Keyboard
    // BEFORE curtainWindow is ever shown so Hyprland treats it as a top-most overlay
    // surface rather than a workspace window that can be tiled or shrunk.
    configureLayerOverlayWindow(curtainWindow);

    auto syncOverlayWindows = [mainWindow, curtainWindow, &backend]() {
        const bool overlayOn = backend.active() || backend.pixelRefresherActive();
        if (curtainWindow) {
            if (overlayOn) {
                configureLayerOverlayWindow(curtainWindow);
#ifdef Q_OS_WIN
                curtainWindow->showFullScreen();
                HWND hwnd = (HWND)curtainWindow->winId();
                if (hwnd) {
                    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
                }
#else
                curtainWindow->setVisible(true);
#endif
                curtainWindow->raise();
                curtainWindow->requestActivate();
                curtainWindow->setKeyboardGrabEnabled(true);
            } else {
                curtainWindow->setKeyboardGrabEnabled(false);
                curtainWindow->setVisible(false);
            }
        }
        if (mainWindow) {
            mainWindow->setKeyboardGrabEnabled(overlayOn);
            if (!overlayOn) {
                mainWindow->raise();
                mainWindow->requestActivate();
            }
        }
    };

    QObject::connect(&backend, &Backend::activeChanged, &app, syncOverlayWindows);
    QObject::connect(&backend, &Backend::pixelRefresherChanged, &app, syncOverlayWindows);
    QObject::connect(&backend, &Backend::reinforceOverlayRequested, &app, syncOverlayWindows);

    if (startNow) {
        QTimer::singleShot(0, &backend, &Backend::startBlackout);
    }

    return app.exec();
}
