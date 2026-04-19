#pragma once

#include <QString>
#include <QList>
#include <vector>
#include <memory>

class Entity;

/// Parses DXF files and returns Entity objects for the Graphics Design system.
class DxfParser
{
public:
    DxfParser() = default;

    /// Parse a DXF file. Returns list of entities. Returns empty list on failure.
    std::vector<std::unique_ptr<Entity>> parse(const QString& filePath);

    /// Get error message if parsing failed.
    QString errorMessage() const { return m_error; }

private:
    struct GroupPair {
        int code;
        QString value;
    };

    QList<GroupPair> readGroupPairs(const QString& filePath);
    void parseEntitiesSection(const QList<GroupPair>& pairs, int startIdx,
                              std::vector<std::unique_ptr<Entity>>& entities);

    std::unique_ptr<Entity> parseLine(const QList<GroupPair>& pairs, int& idx);
    std::unique_ptr<Entity> parseCircle(const QList<GroupPair>& pairs, int& idx);
    std::unique_ptr<Entity> parseArc(const QList<GroupPair>& pairs, int& idx);
    std::unique_ptr<Entity> parseLwPolyline(const QList<GroupPair>& pairs, int& idx);
    std::unique_ptr<Entity> parsePolyline(const QList<GroupPair>& pairs, int& idx);
    std::unique_ptr<Entity> parseSpline(const QList<GroupPair>& pairs, int& idx);
    std::unique_ptr<Entity> parseEllipse(const QList<GroupPair>& pairs, int& idx);

    QString m_error;
};
