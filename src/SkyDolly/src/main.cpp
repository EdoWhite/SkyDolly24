/**
 * Sky Dolly - The Black Sheep for Your Flight Recordings
 *
 * Copyright (c) 2020 - 2025 Oliver Knoll
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
 * to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <memory>
#include <exception>

#include <QCoreApplication>
#include <QSysInfo>
#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QStringList>
#include <QString>
#include <QDebug>
#include <QStyleFactory>
#include <QStringBuilder>
#include <QMessageBox>
#ifdef DEBUG
#include <QDebug>
#endif

#include <QDir>

#include <Kernel/Version.h>
#include <Kernel/AddonInstaller.h>
#include <Kernel/StackTrace.h>
#include <Kernel/Log.h>
#include <Kernel/Settings.h>
#include <Kernel/System.h>
#include <Kernel/RecentFile.h>
#include <Model/Logbook.h>
#include <PluginManager/SkyConnectManager.h>
#include <Persistence/PersistenceManager.h>
#include <PluginManager/PluginManager.h>
#include <Remote/RemoteServer.h>
#include <UserInterface/MainWindow.h>
#include "ExceptionHandler.h"
#include "SignalHandler.h"
#include "CrashHandler.h"
#include "SingleInstance.h"
#include "ErrorCodes.h"

static void destroySingletons() noexcept
{
    Logbook::destroyInstance();
    PersistenceManager::destroyInstance();
    PluginManager::destroyInstance();
    SkyConnectManager::destroyInstance();
    RecentFile::destroyInstance();

    // Destroying the settings singleton also persists the settings; destroy this instance
    // last, as previous plugin managers such as the SkyConnectManager may still want
    // to store their plugin settings
    Settings::destroyInstance();
}

int main(int argc, char **argv) noexcept
{
    std::set_terminate(ExceptionHandler::onTerminate);
    // Catches the failures that std::set_terminate cannot see: access violations, stack overflows
    // and other structured exceptions, which otherwise kill the process without a trace
    CrashHandler::install();

    QCoreApplication::setOrganizationName(Version::getOrganisationName());
    QCoreApplication::setApplicationName(Version::getApplicationName());
    QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus);

    QApplication application(argc, argv);

    // Must come after the organisation/application names have been set: they determine the
    // location of the log directory
    Log::initialise();

    // Set the user interface style (if not default)
    // Implementation note: must be set AFTER QApplication instantiation
    const QString styleKey = Settings::getInstance().getStyleKey();
    if (styleKey != Settings::DefaultStyleKey) {
        QApplication::setStyle(styleKey);
    }

    // Signals must be registered after the QApplication instantiation, due
    // to the QSocketNotifier
    SignalHandler signalHandler;
    signalHandler.registerSignals();

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QCoreApplication::translate("main",
            "Records and replays flights in Microsoft Flight Simulator 2024.\n\n"
            "With no options the desktop window is shown. The simulator starts Sky Dolly with "
            "--engine, in which case the window stays hidden and the panel in the simulator's "
            "toolbar is the interface."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption engineOption {
        QStringList {"engine"},
        QCoreApplication::translate("main", "Start without showing the window, in the notification area.")
    };
    parser.addOption(engineOption);

    const QCommandLineOption portOption {
        QStringList {"port"},
        QCoreApplication::translate("main", "Serve the in-game panel API on <port> (default %1).")
            .arg(RemoteServer::DefaultPort),
        QCoreApplication::translate("main", "port")
    };
    parser.addOption(portOption);

    const QCommandLineOption noServerOption {
        QStringList {"no-panel-server"},
        QCoreApplication::translate("main", "Do not serve the in-game panel API at all.")
    };
    parser.addOption(noServerOption);

    const QCommandLineOption installOption {
        QStringList {"install-addon"},
        QCoreApplication::translate("main",
            "Install the toolbar panel into MSFS 2024 and have the simulator start Sky Dolly.")
    };
    parser.addOption(installOption);

    const QCommandLineOption uninstallOption {
        QStringList {"uninstall-addon"},
        QCoreApplication::translate("main", "Undo --install-addon.")
    };
    parser.addOption(uninstallOption);

    parser.addPositionalArgument(
        QCoreApplication::translate("main", "logbook"),
        QCoreApplication::translate("main", "The logbook to open. The last used one if omitted."));

    parser.process(application);

    if (parser.isSet(installOption) || parser.isSet(uninstallOption)) {
        const bool installing = parser.isSet(installOption);
        const QString executablePath = QCoreApplication::applicationFilePath();
        const QString packageSourcePath =
            QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("Community/skydolly-panel");

        const AddonInstaller::Result result = installing ?
            AddonInstaller::install(executablePath, packageSourcePath) :
            AddonInstaller::uninstall();

        // A GUI application on Windows has no console to write to, so the outcome has to be shown
        // where it will actually be seen; it also goes to the log for a scripted install
        const QString report = result.messages.join('\n');
        for (const QString &message : result.messages) {
            qInfo().noquote() << message;
        }
        if (result.ok) {
            QMessageBox::information(nullptr, QCoreApplication::translate("main", "Sky Dolly"),
                                     installing ?
                                         QCoreApplication::translate("main", "Sky Dolly is now installed in Microsoft Flight Simulator 2024.\n\n%1").arg(report) :
                                         QCoreApplication::translate("main", "Sky Dolly has been removed from Microsoft Flight Simulator 2024.\n\n%1").arg(report));
        } else {
            QMessageBox::warning(nullptr, QCoreApplication::translate("main", "Sky Dolly"),
                                 QCoreApplication::translate("main", "The operation did not complete.\n\n%1").arg(report));
        }
        return result.ok ? ErrorCodes::Ok : ErrorCodes::UnknownError;
    }

    const QStringList positionalArguments = parser.positionalArguments();
    const QString filePath = positionalArguments.isEmpty() ? QString() : positionalArguments.constFirst();
    const bool engineMode = parser.isSet(engineOption);

    quint16 panelServerPort {RemoteServer::DefaultPort};
    if (parser.isSet(portOption)) {
        bool ok {false};
        const uint port = parser.value(portOption).toUInt(&ok);
        if (!ok || port == 0 || port > 65535) {
            qCritical() << "Not a valid port:" << parser.value(portOption);
            return ErrorCodes::InvalidArgument;
        }
        panelServerPort = static_cast<quint16>(port);
    }

    // The simulator launches Sky Dolly on its own now, so a user double-clicking the shortcut while
    // it is already running is routine rather than unlikely - and two processes opening the same
    // SQLite logbook is a good way to lose a recording.
    SingleInstance singleInstance;
    if (!singleInstance.tryBecomePrimary(filePath)) {
        return ErrorCodes::Ok;
    }

    // In engine mode nothing is on screen, so closing the last dialog must not end the process
    QApplication::setQuitOnLastWindowClosed(!engineMode);

    int res {ErrorCodes::Ok};
    try {
        // Main window scope
        {
            std::unique_ptr<MainWindow> mainWindow = std::make_unique<MainWindow>(filePath);
            if (engineMode) {
                mainWindow->enterEngineMode();
            } else {
                mainWindow->show();
            }

            // A second launch brings this instance up rather than starting another one; in engine
            // mode that is the only way the user can reach the window without the tray icon
            QObject::connect(&singleInstance, &SingleInstance::anotherInstanceStarted,
                             mainWindow.get(), [&mainWindow](const QString &logbookPath) noexcept {
                if (!logbookPath.isEmpty()) {
                    mainWindow->connectWithLogbook(logbookPath);
                }
                mainWindow->showFromTray();
            });

            // The in-game panel drives Sky Dolly over this. It is started after the main window so
            // that the plugins and the logbook it reports on are already in place, and a failure
            // to bind is not fatal: the desktop window remains perfectly usable without it.
            RemoteServer remoteServer;
            if (!parser.isSet(noServerOption) && !remoteServer.start(panelServerPort)) {
                qWarning() << "The in-game panel will not be able to reach Sky Dolly:"
                           << remoteServer.getLastError();
            }

            res = application.exec();
        }
        // Destroy singletons after main window has been deleted
        destroySingletons();
    } catch (const std::exception &ex) {
        const QString stackTrace = StackTrace::generate();
        ExceptionHandler::onError("Exception", stackTrace, ex);
        res = ErrorCodes::StandardException;
    } catch (...) {
        const QString stackTrace = StackTrace::generate();
        ExceptionHandler::onError("Exception", stackTrace, "Non std::exception");
        res = ErrorCodes::UnknownException;
    }

    Log::shutdown();
    return res;
}
