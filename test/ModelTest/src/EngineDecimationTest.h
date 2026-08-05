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
#ifndef ENGINEDECIMATIONTEST_H
#define ENGINEDECIMATIONTEST_H

#include <QObject>

/*!
 * Test cases for the engine sample decimation.
 *
 * Recording the engine speeds means the engine record changes on every frame, where before it only
 * changed when a lever moved. Dropping most of those samples again is only defensible if nothing
 * that matters is dropped with them, so that is what these tests are about: the discrete events a
 * replay has to hit exactly, and the bound on how far a dropped engine speed may have drifted.
 */
class EngineDecimationTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() noexcept;
    void cleanupTestCase() noexcept;

    void dropsAnUnchangingIdle() noexcept;
    void alwaysStoresAfterTheMaximumInterval() noexcept;
    void storesEveryDiscreteStateChange() noexcept;
    void storesEveryLeverMovement() noexcept;
    void storesOnceTheEngineSpeedHasDrifted() noexcept;
    void storesOnceTheTurbineSpoolHasDrifted() noexcept;
    void storesASampleThatGoesBackInTime() noexcept;

    void decimatedSpoolUpStaysWithinThreshold() noexcept;
    void aSteadyCruiseCostsOneSamplePerSecond() noexcept;
};

#endif // ENGINEDECIMATIONTEST_H
