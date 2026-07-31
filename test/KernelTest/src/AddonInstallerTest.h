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
#ifndef ADDONINSTALLERTEST_H
#define ADDONINSTALLERTEST_H

#include <QObject>

/*!
 * Test cases for the EXE.xml manipulation behind the MSFS add-on registration.
 *
 * EXE.xml is shared with every other add-on installed in the simulator and is occasionally reset
 * by a simulator update. Damaging it means breaking somebody else's product, so the cases that
 * matter are the ones where the file is not what we expect: absent, already carrying other
 * add-ons, already carrying ours, or malformed.
 */
class AddonInstallerTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() noexcept;
    void cleanupTestCase() noexcept;

    void addToAbsentFile() noexcept;
    void addKeepsOtherAddons() noexcept;
    void addIsIdempotent() noexcept;
    void addUpdatesOwnPathInPlace() noexcept;
    void addRejectsMalformedXml() noexcept;

    void removeLeavesOtherAddons() noexcept;
    void removeIsSafeWhenAbsent() noexcept;
    void removeRejectsMalformedXml() noexcept;

    void hasLaunchAddon_data() noexcept;
    void hasLaunchAddon() noexcept;

    void addThenRemoveRoundTrips() noexcept;
};

#endif // ADDONINSTALLERTEST_H
