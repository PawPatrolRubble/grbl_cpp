#include "PlotJob.h"
#include <QCryptographicHash>
#include <QDataStream>
#include <QIODevice>

QByteArray PlotJob::executionDigest() const
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream << id << automaticRack << initialHolderKnown << initialToolId << returnFinalPen
           << penSequence << configurationFingerprint << workCoordinateSystem << safeZ << phaseTimeoutMs;
    for (double value : workMinimum) stream << value;
    for (double value : workMaximum) stream << value;
    for (const auto& block : blocks)
        stream << block.text << block.sourceLine << block.pathId << block.toolId
               << int(block.kind) << int(block.frame) << block.phase << int(block.holderEffect) << block.timeoutMs;
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

QStringList PlotJob::lines() const
{
    QStringList result;
    result.reserve(blocks.size());
    for (const auto& block : blocks) result << block.text;
    return result;
}

bool PlotJob::containsRackOperations() const
{
    for (const auto& block : blocks)
        if (block.kind == PlotJobBlock::Kind::Rack) return true;
    return false;
}

QStringList PlotJob::drawingPreviewLines() const
{
    if (!automaticRack) return lines();
    QStringList result;
    result.reserve(blocks.size());
    for (const auto& block : blocks)
        result << ((block.kind == PlotJobBlock::Kind::Drawing
                    || (block.kind == PlotJobBlock::Kind::Setup && block.frame == PlotJobBlock::Frame::Work))
                   ? block.text : QString{});
    return result; // Keep every source index even when omitting machine-frame motion.
}

PlotJob PlotJob::fromLines(const QStringList& lines)
{
    PlotJob job;
    for (int i = 0; i < lines.size(); ++i)
        job.blocks.append({lines[i], i, {}, {}, PlotJobBlock::Kind::Raw,
                           PlotJobBlock::Frame::Unspecified});
    return job;
}
