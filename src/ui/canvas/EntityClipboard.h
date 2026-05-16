#pragma once

#include "../../grbl/GraphicsDesign/Document.h"
#include "../../grbl/GraphicsDesign/CommandStack.h"

#include <QPointF>
#include <memory>
#include <vector>

/// Manages copy/paste/duplicate operations for canvas entities.
/// Handles entity cloning with offset positioning and command stack integration.
class EntityClipboard
{
public:
    explicit EntityClipboard(Document& document, CommandStack& commandStack)
        : m_document(document)
        , m_commandStack(commandStack)
    {}

    /// Copy the currently selected entity to the clipboard.
    void copy();

    /// Paste the clipboard entity with an offset based on paste count.
    void paste();

    /// Duplicate the currently selected entity with a standard offset.
    void duplicate();

    /// Clear the clipboard.
    void clear() { m_copiedEntities.clear(); m_pasteSerial = 0; }

    /// Check if clipboard has content.
    bool hasContent() const { return !m_copiedEntities.empty(); }

private:
    void addEntities(std::vector<std::unique_ptr<Entity>> entities);

    static QPointF shortcutOffsetForStep(int step) {
        constexpr double kOffset = 10.0;
        return QPointF(kOffset * std::max(step, 1), kOffset * std::max(step, 1));
    }

    Document& m_document;
    CommandStack& m_commandStack;
    std::vector<std::unique_ptr<Entity>> m_copiedEntities;
    int m_pasteSerial = 0;
};
