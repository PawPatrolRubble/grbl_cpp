#pragma once

#include "../Document.h"
#include "../CommandStack.h"

#include <QPainter>
#include <QString>
#include <QPointF>

/// Base class for drawing/editing tools.
/// Tools handle user interaction with the canvas and modify the document
/// through the CommandStack for undo/redo support.
class Tool
{
public:
    Tool(Document& document, CommandStack& commandStack)
        : m_document(document)
        , m_commandStack(commandStack)
    {}

    virtual ~Tool() = default;

    /// Tool name for UI display
    virtual QString name() const = 0;

    /// Instructions for the user (shown in status bar)
    virtual QString instructions() const = 0;

    /// Handle mouse press (world coordinates)
    virtual void mousePress(const QPointF& pos) = 0;

    /// Handle mouse move (world coordinates)
    virtual void mouseMove(const QPointF& pos) = 0;

    /// Handle mouse release (world coordinates)
    virtual void mouseRelease(const QPointF& pos) = 0;

    /// Cancel current operation (Esc key or right-click)
    virtual void cancel() = 0;

    /// Paint preview of current operation
    virtual void paintPreview(QPainter& painter) const = 0;

    /// Check if tool is in an active operation
    virtual bool isActive() const = 0;

protected:
    Document& document() const { return m_document; }
    CommandStack& commandStack() const { return m_commandStack; }

private:
    Document& m_document;
    CommandStack& m_commandStack;
};