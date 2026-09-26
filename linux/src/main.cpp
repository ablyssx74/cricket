/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#include "config.h"
#include "mainwindow.h"
#include "services.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("cricket");
    QApplication::setOrganizationName("cricket");
    QApplication::setApplicationDisplayName("Cricket");
    QApplication::setApplicationVersion(AppInfo::VERSION_NUMBER);
    // Lets KDE Plasma / Wayland match the window to cricket.desktop (icon, taskbar grouping).
    QGuiApplication::setDesktopFileName("cricket");
    QApplication::setWindowIcon(QIcon::fromTheme("cricket", QIcon(":/cricket.svg")));

    QCommandLineParser parser;
    parser.setApplicationDescription("Cricket - a lightweight multi-server IRC client");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption noConnect("no-autoconnect", "Do not connect to networks marked 'connect on startup'.");
    QCommandLineOption debug("debug", "Print raw IRC traffic to the terminal.");
    parser.addOption(noConnect);
    parser.addOption(debug);
    parser.process(app);

    loadConfig();
    if (parser.isSet(debug))
        cfg.debugEnable = true;

    MainWindow window;
    window.show();
    if (!parser.isSet(noConnect))
        window.autoConnect();

    return app.exec();
}
