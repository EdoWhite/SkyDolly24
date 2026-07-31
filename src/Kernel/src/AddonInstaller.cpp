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
#include <QObject>
#include <QString>
#include <QStringBuilder>
#include <QStringList>
#include <QDir>
#include <QDirIterator>
#include <QFileInfoList>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QTextStream>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>

#include "AddonInstaller.h"

namespace
{
    constexpr const char *ExeXmlFileName {"EXE.xml"};
    constexpr const char *UserCfgFileName {"UserCfg.opt"};
    constexpr const char *PackageName {"skydolly-panel"};
    constexpr const char *LaunchAddonTag {"Launch.Addon"};
    constexpr const char *EngineCommandLine {"--engine"};

    QString roamingAppDataPath() noexcept
    {
        return QString::fromLocal8Bit(qgetenv("APPDATA"));
    }

    QString localAppDataPath() noexcept
    {
        return QString::fromLocal8Bit(qgetenv("LOCALAPPDATA"));
    }

    // Paths are compared to decide whether an existing entry is ours; Windows is case insensitive
    // and mixes separators freely, so normalise before comparing
    QString normalisePath(const QString &path) noexcept
    {
        return QDir::cleanPath(path).toLower();
    }

    QDomElement appendTextElement(QDomDocument &document, QDomElement &parent,
                                  const QString &tagName, const QString &text) noexcept
    {
        QDomElement element = document.createElement(tagName);
        element.appendChild(document.createTextNode(text));
        parent.appendChild(element);
        return element;
    }

    QString childText(const QDomElement &element, const QString &tagName) noexcept
    {
        const QDomNodeList children = element.elementsByTagName(tagName);
        return children.isEmpty() ? QString() : children.at(0).toElement().text().trimmed();
    }

    // A minimal but complete Launch document, for the case where EXE.xml does not exist yet or a
    // simulator update has emptied it
    QDomDocument createEmptyLaunchDocument() noexcept
    {
        QDomDocument document;
        document.appendChild(document.createProcessingInstruction(
            QStringLiteral("xml"), QStringLiteral("version=\"1.0\" encoding=\"UTF-8\"")));
        QDomElement root = document.createElement(QStringLiteral("SimBase.Document"));
        root.setAttribute(QStringLiteral("Type"), QStringLiteral("Launch"));
        root.setAttribute(QStringLiteral("version"), QStringLiteral("1,0"));
        document.appendChild(root);
        ::appendTextElement(document, root, QStringLiteral("Descr"), QStringLiteral("Launch"));
        ::appendTextElement(document, root, QStringLiteral("Filename"), QString::fromLatin1(::ExeXmlFileName));
        ::appendTextElement(document, root, QStringLiteral("Disabled"), QStringLiteral("False"));
        ::appendTextElement(document, root, QStringLiteral("Launch.ManualLoad"), QStringLiteral("False"));
        return document;
    }

    bool parseOrCreate(const QString &exeXml, QDomDocument &document) noexcept
    {
        if (exeXml.trimmed().isEmpty()) {
            document = ::createEmptyLaunchDocument();
            return true;
        }
        // A malformed EXE.xml must not be silently replaced: another add-on's registration would
        // disappear with it, and the user would have no idea why
        return document.setContent(exeXml).errorMessage.isEmpty();
    }

    QDomElement findRoot(QDomDocument &document) noexcept
    {
        QDomElement root = document.documentElement();
        if (root.isNull()) {
            root = document.createElement(QStringLiteral("SimBase.Document"));
            root.setAttribute(QStringLiteral("Type"), QStringLiteral("Launch"));
            root.setAttribute(QStringLiteral("version"), QStringLiteral("1,0"));
            document.appendChild(root);
        }
        return root;
    }
}

// PUBLIC

QString AddonInstaller::findExeXmlPath() noexcept
{
    const QString dataPath = getSimulatorDataPath();
    return dataPath.isEmpty() ? QString() : QDir(dataPath).absoluteFilePath(::ExeXmlFileName);
}

QString AddonInstaller::findCommunityFolder() noexcept
{
    const QString dataPath = getSimulatorDataPath();
    if (dataPath.isEmpty()) {
        return {};
    }

    // UserCfg.opt carries a line: InstalledPackagesPath "D:\MSFS2024\Packages". The Community
    // folder lives underneath it, and the user may well have moved it to another drive, so the
    // file is the only reliable source.
    QFile userCfg {QDir(dataPath).absoluteFilePath(::UserCfgFileName)};
    if (userCfg.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream {&userCfg};
        while (!stream.atEnd()) {
            const QString line = stream.readLine().trimmed();
            if (line.startsWith(QStringLiteral("InstalledPackagesPath"), Qt::CaseInsensitive)) {
                const auto firstQuote = line.indexOf('"');
                const auto lastQuote = line.lastIndexOf('"');
                if (firstQuote >= 0 && lastQuote > firstQuote) {
                    const QString packagesPath = line.mid(firstQuote + 1, lastQuote - firstQuote - 1);
                    return QDir(packagesPath).absoluteFilePath(QStringLiteral("Community"));
                }
            }
        }
    }

    // Fall back to the default layout next to the configuration
    const QString fallback = QDir(dataPath).absoluteFilePath(QStringLiteral("Packages/Community"));
    return QDir(fallback).exists() ? fallback : QString();
}

bool AddonInstaller::addLaunchAddon(const QString &exeXml, const QString &executablePath,
                                    const QString &commandLine, QString &result) noexcept
{
    QDomDocument document;
    if (!::parseOrCreate(exeXml, document)) {
        return false;
    }
    QDomElement root = ::findRoot(document);

    // Idempotent: replace our own entry rather than adding a second one, so that repairing a
    // registration is just running the install again
    const QDomNodeList addons = root.elementsByTagName(QString::fromLatin1(::LaunchAddonTag));
    for (int i = addons.count() - 1; i >= 0; --i) {
        QDomElement addon = addons.at(i).toElement();
        if (::childText(addon, QStringLiteral("Name")) == QLatin1StringView(AddonName)) {
            root.removeChild(addon);
        }
    }

    QDomElement addon = document.createElement(QString::fromLatin1(::LaunchAddonTag));
    root.appendChild(addon);
    ::appendTextElement(document, addon, QStringLiteral("Name"), QString::fromLatin1(AddonName));
    ::appendTextElement(document, addon, QStringLiteral("Disabled"), QStringLiteral("False"));
    ::appendTextElement(document, addon, QStringLiteral("ManualLoad"), QStringLiteral("False"));
    ::appendTextElement(document, addon, QStringLiteral("Path"), QDir::toNativeSeparators(executablePath));
    if (!commandLine.isEmpty()) {
        ::appendTextElement(document, addon, QStringLiteral("CommandLine"), commandLine);
    }

    result = document.toString(4);
    return true;
}

bool AddonInstaller::removeLaunchAddon(const QString &exeXml, QString &result) noexcept
{
    QDomDocument document;
    if (exeXml.trimmed().isEmpty()) {
        // Nothing to remove, and nothing to write back either
        result = exeXml;
        return true;
    }
    if (!document.setContent(exeXml).errorMessage.isEmpty()) {
        return false;
    }

    QDomElement root = document.documentElement();
    if (root.isNull()) {
        result = exeXml;
        return true;
    }

    const QDomNodeList addons = root.elementsByTagName(QString::fromLatin1(::LaunchAddonTag));
    for (int i = addons.count() - 1; i >= 0; --i) {
        QDomElement addon = addons.at(i).toElement();
        if (::childText(addon, QStringLiteral("Name")) == QLatin1StringView(AddonName)) {
            addon.parentNode().removeChild(addon);
        }
    }

    result = document.toString(4);
    return true;
}

bool AddonInstaller::hasLaunchAddon(const QString &exeXml, const QString &executablePath) noexcept
{
    QDomDocument document;
    if (exeXml.trimmed().isEmpty() || !document.setContent(exeXml).errorMessage.isEmpty()) {
        return false;
    }
    const QDomElement root = document.documentElement();
    if (root.isNull()) {
        return false;
    }

    const QDomNodeList addons = root.elementsByTagName(QString::fromLatin1(::LaunchAddonTag));
    for (int i = 0; i < addons.count(); ++i) {
        const QDomElement addon = addons.at(i).toElement();
        if (::childText(addon, QStringLiteral("Name")) == QLatin1StringView(AddonName) &&
            ::normalisePath(::childText(addon, QStringLiteral("Path"))) == ::normalisePath(executablePath)) {
            return true;
        }
    }
    return false;
}

bool AddonInstaller::writeLayout(const QString &packagePath) noexcept
{
    QDir packageDir {packagePath};
    if (!packageDir.exists()) {
        return false;
    }

    QJsonArray content;
    QDirIterator it {packagePath, QDir::Files, QDirIterator::Subdirectories};
    while (it.hasNext()) {
        const QFileInfo fileInfo {it.next()};
        const QString relativePath = packageDir.relativeFilePath(fileInfo.absoluteFilePath());
        if (relativePath == QLatin1StringView("layout.json") ||
            relativePath == QLatin1StringView("manifest.json")) {
            continue;
        }
        QJsonObject entry;
        entry.insert("path", QString(relativePath).replace('\\', '/'));
        entry.insert("size", fileInfo.size());
        // The simulator expects a Windows FILETIME: 100 ns intervals since 1601-01-01
        constexpr qint64 SecondsFrom1601To1970 {11644473600LL};
        entry.insert("date", (fileInfo.lastModified().toUTC().toSecsSinceEpoch() + SecondsFrom1601To1970) * 10000000LL);
        content.append(entry);
    }

    QJsonObject layout;
    layout.insert("content", content);

    QFile layoutFile {packageDir.absoluteFilePath(QStringLiteral("layout.json"))};
    if (!layoutFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    layoutFile.write(QJsonDocument(layout).toJson(QJsonDocument::Indented));
    return true;
}

AddonInstaller::Result AddonInstaller::install(const QString &executablePath,
                                               const QString &packageSourcePath) noexcept
{
    Result result;

    const QString communityFolder = findCommunityFolder();
    if (communityFolder.isEmpty()) {
        result.add(QObject::tr("No Microsoft Flight Simulator 2024 installation was found."));
        return result;
    }
    if (!QDir(packageSourcePath).exists()) {
        result.add(QObject::tr("The panel package is missing: %1").arg(packageSourcePath));
        return result;
    }

    // Community package
    const QString destination = QDir(communityFolder).absoluteFilePath(QString::fromLatin1(::PackageName));
    QDir destinationDir {destination};
    if (destinationDir.exists() && !destinationDir.removeRecursively()) {
        result.add(QObject::tr("The previous panel package could not be removed: %1").arg(destination));
        return result;
    }
    if (!copyDirectory(packageSourcePath, destination)) {
        result.add(QObject::tr("The panel package could not be copied to %1").arg(destination));
        return result;
    }
    if (!writeLayout(destination)) {
        result.add(QObject::tr("layout.json could not be written; the simulator will refuse the package."));
        return result;
    }
    result.add(QObject::tr("Panel package installed in %1").arg(destination));

    // EXE.xml
    const QString exeXmlPath = findExeXmlPath();
    if (exeXmlPath.isEmpty()) {
        result.add(QObject::tr("EXE.xml could not be located; Sky Dolly will not start on its own."));
        return result;
    }

    QString exeXml;
    QFile exeXmlFile {exeXmlPath};
    if (exeXmlFile.exists()) {
        if (!exeXmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            result.add(QObject::tr("EXE.xml could not be read: %1").arg(exeXmlPath));
            return result;
        }
        exeXml = QString::fromUtf8(exeXmlFile.readAll());
        exeXmlFile.close();

        // EXE.xml is shared with every other add-on: never touch it without a way back
        const QString backupPath = exeXmlPath % QStringLiteral(".skydolly-backup");
        QFile::remove(backupPath);
        if (QFile::copy(exeXmlPath, backupPath)) {
            result.add(QObject::tr("Backed up EXE.xml to %1").arg(backupPath));
        }
    }

    QString updated;
    if (!addLaunchAddon(exeXml, executablePath, QString::fromLatin1(::EngineCommandLine), updated)) {
        result.add(QObject::tr("EXE.xml could not be parsed and was left untouched: %1").arg(exeXmlPath));
        return result;
    }
    if (!exeXmlFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        result.add(QObject::tr("EXE.xml could not be written: %1").arg(exeXmlPath));
        return result;
    }
    exeXmlFile.write(updated.toUtf8());
    exeXmlFile.close();
    result.add(QObject::tr("Registered with the simulator in %1").arg(exeXmlPath));

    result.ok = true;
    return result;
}

AddonInstaller::Result AddonInstaller::uninstall() noexcept
{
    Result result;

    const QString communityFolder = findCommunityFolder();
    if (!communityFolder.isEmpty()) {
        const QString destination = QDir(communityFolder).absoluteFilePath(QString::fromLatin1(::PackageName));
        QDir destinationDir {destination};
        if (destinationDir.exists()) {
            if (destinationDir.removeRecursively()) {
                result.add(QObject::tr("Removed %1").arg(destination));
            } else {
                result.add(QObject::tr("The panel package could not be removed: %1").arg(destination));
                return result;
            }
        }
    }

    const QString exeXmlPath = findExeXmlPath();
    QFile exeXmlFile {exeXmlPath};
    if (!exeXmlPath.isEmpty() && exeXmlFile.exists()) {
        if (!exeXmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            result.add(QObject::tr("EXE.xml could not be read: %1").arg(exeXmlPath));
            return result;
        }
        const QString exeXml = QString::fromUtf8(exeXmlFile.readAll());
        exeXmlFile.close();

        QString updated;
        if (!removeLaunchAddon(exeXml, updated)) {
            result.add(QObject::tr("EXE.xml could not be parsed and was left untouched: %1").arg(exeXmlPath));
            return result;
        }
        if (!exeXmlFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            result.add(QObject::tr("EXE.xml could not be written: %1").arg(exeXmlPath));
            return result;
        }
        exeXmlFile.write(updated.toUtf8());
        exeXmlFile.close();
        result.add(QObject::tr("Removed the Sky Dolly entry from %1").arg(exeXmlPath));
    }

    result.ok = true;
    return result;
}

// PRIVATE

QString AddonInstaller::getSimulatorDataPath() noexcept
{
    // Steam first, then MS Store: the same order FlightSimulator::isInstalled uses
    const QString steamPath = ::roamingAppDataPath() % QStringLiteral("/Microsoft Flight Simulator 2024");
    if (QDir(steamPath).exists()) {
        return steamPath;
    }
    const QString storePath = ::localAppDataPath() %
                              QStringLiteral("/Packages/Microsoft.Limitless_8wekyb3d8bbwe/LocalCache");
    if (QDir(storePath).exists()) {
        return storePath;
    }
    return {};
}

bool AddonInstaller::copyDirectory(const QString &source, const QString &destination) noexcept
{
    QDir sourceDir {source};
    if (!sourceDir.exists()) {
        return false;
    }
    if (!QDir().mkpath(destination)) {
        return false;
    }

    const QFileInfoList entries = sourceDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        const QString target = QDir(destination).absoluteFilePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyDirectory(entry.absoluteFilePath(), target)) {
                return false;
            }
        } else {
            QFile::remove(target);
            if (!QFile::copy(entry.absoluteFilePath(), target)) {
                return false;
            }
        }
    }
    return true;
}
