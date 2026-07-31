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
#include <QtTest>
#include <QString>

#include <Kernel/AddonInstaller.h>

#include "AddonInstallerTest.h"

namespace
{
    // A file as another add-on would leave it: the entry that must survive everything we do
    const QString OtherAddonXml {QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<SimBase.Document Type=\"Launch\" version=\"1,0\">\n"
        "    <Descr>Launch</Descr>\n"
        "    <Filename>EXE.xml</Filename>\n"
        "    <Disabled>False</Disabled>\n"
        "    <Launch.ManualLoad>False</Launch.ManualLoad>\n"
        "    <Launch.Addon>\n"
        "        <Name>GSX</Name>\n"
        "        <Disabled>False</Disabled>\n"
        "        <Path>C:\\Program Files\\Addon Manager\\couatl64_MSFS.exe</Path>\n"
        "    </Launch.Addon>\n"
        "</SimBase.Document>\n")};

    const QString SkyDollyPath {QStringLiteral("C:/Program Files/SkyDolly/SkyDolly.exe")};

    int countOccurrences(const QString &text, const QString &needle)
    {
        int count {0};
        qsizetype from {0};
        while ((from = text.indexOf(needle, from)) >= 0) {
            ++count;
            from += needle.size();
        }
        return count;
    }
}

// PRIVATE SLOTS

void AddonInstallerTest::initTestCase() noexcept
{}

void AddonInstallerTest::cleanupTestCase() noexcept
{}

void AddonInstallerTest::addToAbsentFile() noexcept
{
    // Setup: EXE.xml does not exist yet, which is the common case on a fresh installation
    QString result;

    // Exercise
    const bool ok = AddonInstaller::addLaunchAddon({}, ::SkyDollyPath, "--engine", result);

    // Verify
    QVERIFY(ok);
    QVERIFY(result.contains("SimBase.Document"));
    QVERIFY(result.contains("Launch.Addon"));
    QVERIFY(result.contains(AddonInstaller::AddonName));
    QVERIFY(result.contains("--engine"));
    QVERIFY(AddonInstaller::hasLaunchAddon(result, ::SkyDollyPath));
}

void AddonInstallerTest::addKeepsOtherAddons() noexcept
{
    // Setup
    QString result;

    // Exercise
    const bool ok = AddonInstaller::addLaunchAddon(::OtherAddonXml, ::SkyDollyPath, "--engine", result);

    // Verify: somebody else's registration must come through untouched
    QVERIFY(ok);
    QVERIFY(result.contains("GSX"));
    QVERIFY(result.contains("couatl64_MSFS.exe"));
    QVERIFY(AddonInstaller::hasLaunchAddon(result, ::SkyDollyPath));
    QCOMPARE(::countOccurrences(result, "<Launch.Addon>"), 2);
}

void AddonInstallerTest::addIsIdempotent() noexcept
{
    // Setup: installing twice is how a user repairs a registration a simulator update removed
    QString once;
    QVERIFY(AddonInstaller::addLaunchAddon(::OtherAddonXml, ::SkyDollyPath, "--engine", once));

    // Exercise
    QString twice;
    const bool ok = AddonInstaller::addLaunchAddon(once, ::SkyDollyPath, "--engine", twice);

    // Verify: still exactly one entry of ours, and the other add-on still there
    QVERIFY(ok);
    QCOMPARE(::countOccurrences(twice, AddonInstaller::AddonName), 1);
    QCOMPARE(::countOccurrences(twice, "<Launch.Addon>"), 2);
    QVERIFY(twice.contains("GSX"));
}

void AddonInstallerTest::addUpdatesOwnPathInPlace() noexcept
{
    // Setup: Sky Dolly was registered from one folder and has since been moved
    QString before;
    QVERIFY(AddonInstaller::addLaunchAddon({}, "C:/Old/SkyDolly.exe", "--engine", before));

    // Exercise
    QString after;
    const bool ok = AddonInstaller::addLaunchAddon(before, ::SkyDollyPath, "--engine", after);

    // Verify: the stale path is gone rather than left behind as a second entry
    QVERIFY(ok);
    QCOMPARE(::countOccurrences(after, "<Launch.Addon>"), 1);
    QVERIFY(!after.contains("C:\\Old\\SkyDolly.exe"));
    QVERIFY(AddonInstaller::hasLaunchAddon(after, ::SkyDollyPath));
    QVERIFY(!AddonInstaller::hasLaunchAddon(after, "C:/Old/SkyDolly.exe"));
}

void AddonInstallerTest::addRejectsMalformedXml() noexcept
{
    // Setup: a truncated file, which is what an interrupted write leaves behind
    const QString malformed {"<SimBase.Document Type=\"Launch\"><Launch.Addon><Name>GSX</Name>"};
    QString result {"untouched"};

    // Exercise
    const bool ok = AddonInstaller::addLaunchAddon(malformed, ::SkyDollyPath, "--engine", result);

    // Verify: refusing is the point. Replacing the file would silently drop another add-on's
    // registration, and the user would have no way of knowing why it stopped working.
    QVERIFY(!ok);
    QCOMPARE(result, QString("untouched"));
}

void AddonInstallerTest::removeLeavesOtherAddons() noexcept
{
    // Setup
    QString installed;
    QVERIFY(AddonInstaller::addLaunchAddon(::OtherAddonXml, ::SkyDollyPath, "--engine", installed));

    // Exercise
    QString result;
    const bool ok = AddonInstaller::removeLaunchAddon(installed, result);

    // Verify
    QVERIFY(ok);
    QVERIFY(!AddonInstaller::hasLaunchAddon(result, ::SkyDollyPath));
    QVERIFY(result.contains("GSX"));
    QCOMPARE(::countOccurrences(result, "<Launch.Addon>"), 1);
}

void AddonInstallerTest::removeIsSafeWhenAbsent() noexcept
{
    // Setup / Exercise: uninstalling something that was never installed
    QString fromOther;
    const bool okOther = AddonInstaller::removeLaunchAddon(::OtherAddonXml, fromOther);
    QString fromEmpty;
    const bool okEmpty = AddonInstaller::removeLaunchAddon({}, fromEmpty);

    // Verify
    QVERIFY(okOther);
    QVERIFY(fromOther.contains("GSX"));
    QVERIFY(okEmpty);
}

void AddonInstallerTest::removeRejectsMalformedXml() noexcept
{
    // Setup
    const QString malformed {"<SimBase.Document><Launch.Addon><Name>Sky Dolly</Name>"};
    QString result {"untouched"};

    // Exercise
    const bool ok = AddonInstaller::removeLaunchAddon(malformed, result);

    // Verify
    QVERIFY(!ok);
    QCOMPARE(result, QString("untouched"));
}

void AddonInstallerTest::hasLaunchAddon_data() noexcept
{
    QTest::addColumn<QString>("registeredPath");
    QTest::addColumn<QString>("queriedPath");
    QTest::addColumn<bool>("expected");

    QTest::newRow("exact match")
        << ::SkyDollyPath << ::SkyDollyPath << true;
    QTest::newRow("separators differ")
        << "C:/Program Files/SkyDolly/SkyDolly.exe"
        << "C:\\Program Files\\SkyDolly\\SkyDolly.exe" << true;
    QTest::newRow("case differs, as Windows allows")
        << "C:/Program Files/SkyDolly/SkyDolly.exe"
        << "c:/program files/skydolly/skydolly.exe" << true;
    QTest::newRow("a different folder is not a match")
        << "C:/Program Files/SkyDolly/SkyDolly.exe"
        << "D:/Games/SkyDolly/SkyDolly.exe" << false;
}

void AddonInstallerTest::hasLaunchAddon() noexcept
{
    // Setup
    QFETCH(QString, registeredPath);
    QFETCH(QString, queriedPath);
    QFETCH(bool, expected);

    QString exeXml;
    QVERIFY(AddonInstaller::addLaunchAddon({}, registeredPath, "--engine", exeXml));

    // Exercise
    const bool actual = AddonInstaller::hasLaunchAddon(exeXml, queriedPath);

    // Verify
    QCOMPARE(actual, expected);
}

void AddonInstallerTest::addThenRemoveRoundTrips() noexcept
{
    // Setup / Exercise: install then uninstall must leave no trace of us behind
    QString installed;
    QVERIFY(AddonInstaller::addLaunchAddon(::OtherAddonXml, ::SkyDollyPath, "--engine", installed));
    QString removed;
    QVERIFY(AddonInstaller::removeLaunchAddon(installed, removed));

    // Verify
    QVERIFY(!removed.contains(AddonInstaller::AddonName));
    QVERIFY(!removed.contains("--engine"));
    QVERIFY(removed.contains("GSX"));
}

QTEST_MAIN(AddonInstallerTest)
#include "AddonInstallerTest.moc"
