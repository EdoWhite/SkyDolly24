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
#ifndef ADDONINSTALLER_H
#define ADDONINSTALLER_H

#include <QString>
#include <QStringList>

#include "KernelLib.h"

/*!
 * Registers Sky Dolly with Microsoft Flight Simulator 2024, so that the simulator starts it and
 * shows its panel in the toolbar.
 *
 * Two things have to happen: the Community package has to be copied into the simulator's Community
 * folder, and an entry has to be added to \c EXE.xml, the shared file through which every add-on
 * asks the simulator to launch a process.
 *
 * \c EXE.xml is shared with every other add-on and is occasionally reset by a simulator update, so
 * the rules here are: never rewrite an entry that is not ours, always back the file up before
 * touching it, and make adding our own entry idempotent so that repairing a lost registration is
 * just running the install again.
 *
 * The functions that manipulate \c EXE.xml take and return its contents as a string rather than
 * touching the file system, so that all of the interesting cases - the file being absent, holding
 * other add-ons, already holding ours, or being malformed - are testable without a simulator.
 */
class KERNEL_API AddonInstaller final
{
public:
    /*! The Name element identifying our own entry in EXE.xml. */
    static constexpr const char *AddonName {"Sky Dolly"};

    /*!
     * The outcome of an install or uninstall.
     */
    struct Result
    {
        bool ok {false};
        /*! Human readable account of what happened, for the console and the log. */
        QStringList messages;

        void add(const QString &message) noexcept { messages.append(message); }
    };

    /*!
     * Returns the path of the MSFS 2024 \c EXE.xml, whether or not it exists yet.
     *
     * \return the path; empty if no MSFS 2024 installation was found
     */
    static QString findExeXmlPath() noexcept;

    /*!
     * Returns the path of the MSFS 2024 \c Community folder.
     *
     * The location is not fixed: it is whatever \c InstalledPackagesPath in \c UserCfg.opt points
     * at, which the user may have moved to another drive.
     *
     * \return the path; empty if it could not be determined
     */
    static QString findCommunityFolder() noexcept;

    /*!
     * Adds a \c Launch.Addon entry for \p executablePath to the \c EXE.xml given in \p exeXml.
     *
     * \param exeXml
     *        the current contents of EXE.xml; may be empty, in which case a new document is created
     * \param executablePath
     *        the absolute path of the executable the simulator should launch
     * \param commandLine
     *        the arguments to launch it with
     * \param result
     *        receives the updated contents
     * \return \c true upon success; \c false if \p exeXml could not be parsed, in which case
     *         \p result is left untouched
     */
    static bool addLaunchAddon(const QString &exeXml, const QString &executablePath,
                               const QString &commandLine, QString &result) noexcept;

    /*!
     * Removes our own \c Launch.Addon entry from \p exeXml, leaving every other entry alone.
     *
     * \param exeXml
     *        the current contents of EXE.xml
     * \param result
     *        receives the updated contents
     * \return \c true upon success, including when there was nothing to remove; \c false if
     *         \p exeXml could not be parsed
     */
    static bool removeLaunchAddon(const QString &exeXml, QString &result) noexcept;

    /*!
     * Returns whether \p exeXml already contains our own entry pointing at \p executablePath.
     *
     * Used to tell "already installed" from "installed, but the simulator reset the file" and from
     * "installed, but from a different folder".
     */
    static bool hasLaunchAddon(const QString &exeXml, const QString &executablePath) noexcept;

    /*!
     * Installs the add-on: copies the Community package and registers the executable in EXE.xml.
     *
     * \param executablePath
     *        the absolute path of the running executable
     * \param packageSourcePath
     *        the \c skydolly-panel directory shipped alongside the executable
     */
    static Result install(const QString &executablePath, const QString &packageSourcePath) noexcept;

    /*!
     * Removes the Community package and our own EXE.xml entry, leaving everything else in place.
     */
    static Result uninstall() noexcept;

    /*!
     * Writes the \c layout.json the simulator requires next to \p packagePath, listing every file
     * with its size. A package whose layout does not match what is on disk is refused.
     */
    static bool writeLayout(const QString &packagePath) noexcept;

private:
    static QString getSimulatorDataPath() noexcept;
    static bool copyDirectory(const QString &source, const QString &destination) noexcept;
};

#endif // ADDONINSTALLER_H
