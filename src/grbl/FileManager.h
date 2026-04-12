#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class GcodeParser;

class FileManager : public QObject
{
    Q_OBJECT

public:
    explicit FileManager(QObject *parent = nullptr);

    bool loadFile(const QString &filePath);
    void loadLines(const QString &fileName, const QStringList &lines);
    bool saveFile(const QString &filePath, const QStringList &lines);
    void clear();

    QStringList lines() const;
    QString fileName() const;
    QString filePath() const;
    bool isLoaded() const;

signals:
    void fileLoaded(const QString &fileName);
    void fileSaved(const QString &fileName);
    void loadError(const QString &error);

private:
    QStringList m_lines;
    QString m_fileName;
    QString m_filePath;
    bool m_loaded = false;
};