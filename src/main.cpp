// Sizer — resize a batch of images to one width and save the copies to a folder.

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "cli.h"
#include "portalfilepicker.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments << QString::fromLocal8Bit(argv[i]);

    // Commands for scripts and agents run without a display.
    const bool cli = isCliCommand(arguments.value(1));
    if (cli)
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName("sizer");
    app.setApplicationVersion("0.1.0");
    if (cli)
        return runCli(arguments);

    // Matches the window to sizer.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("sizer");
    app.setWindowIcon(QIcon::fromTheme("sizer"));

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Backend backend(new PortalFilePicker);

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    // "sizer open photo.jpg holiday/" starts with those images; so does
    // "sizer photo.jpg", which is what file managers run.
    QStringList files = arguments.mid(1);
    if (files.value(0) == "open")
        files.removeFirst();
    if (!files.isEmpty())
        backend.addPaths(files);

    return app.exec();
}
