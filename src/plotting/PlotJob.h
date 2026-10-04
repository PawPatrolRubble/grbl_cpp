#pragma once

#include <QList>
#include <QStringList>
#include <QUuid>
#include <QMetaType>
#include <array>

struct PlotJobBlock
{
    enum class Kind { Raw, Setup, Drawing, Rack, Synchronization, Finish };
    enum class Frame { Unspecified, Work, Machine };
    QString text;
    int sourceLine = -1;
    QUuid pathId;
    QUuid toolId;
    Kind kind = Kind::Raw;
    Frame frame = Frame::Unspecified;
    QString phase;
    enum class HolderEffect { None, Empty, Loaded };
    HolderEffect holderEffect = HolderEffect::None; // Only on a completed G4 checkpoint.
    int timeoutMs = 30000;
};

// The program blocks are authoritative. Preview geometry is derived from them.
// Controllers take a copy, freezing the program for the duration of execution.
struct PlotJob
{
    QUuid id = QUuid::createUuid();
    QList<PlotJobBlock> blocks;
    bool automaticRack = false;
    bool initialHolderKnown = false;
    QUuid initialToolId;
    bool returnFinalPen = true;
    QList<QUuid> penSequence;
    QByteArray configurationFingerprint;
    QByteArray executionSeal;
    int workCoordinateSystem = 54;
    double safeZ = 0;
    std::array<double, 3> workMinimum{}, workMaximum{};
    int phaseTimeoutMs = 30000;
    QByteArray executionDigest() const;
    QStringList lines() const;
    QStringList drawingPreviewLines() const;
    bool containsRackOperations() const;
    static PlotJob fromLines(const QStringList& lines);
};

Q_DECLARE_METATYPE(PlotJob)
