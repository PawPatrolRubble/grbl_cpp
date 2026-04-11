#include "FileManager.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

FileManager::FileManager(QObject *parent)
    : QObject(parent)
{
}

bool FileManager::loadFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit loadError(tr("Cannot open file: %1").arg(filePath));
        return false;
    }

    QTextStream in(&file);
    m_lines.clear();
    while (!in.atEnd())
        m_lines.append(in.readLine());

    file.close();

    QFileInfo info(filePath);
    m_fileName = info.fileName();
    m_filePath = info.absoluteFilePath();
    m_loaded = true;

    emit fileLoaded(m_fileName);
    return true;
}

bool FileManager::saveFile(const QString &filePath, const QStringList &lines)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    for (const QString &line : lines)
        out << line << "\n";

    file.close();

    QFileInfo info(filePath);
    m_fileName = info.fileName();
    m_filePath = info.absoluteFilePath();
    m_lines = lines;
    m_loaded = true;

    emit fileSaved(m_fileName);
    return true;
}

void FileManager::clear()
{
    m_lines.clear();
    m_fileName.clear();
    m_filePath.clear();
    m_loaded = false;
}

QStringList FileManager::lines() const { return m_lines; }
QString FileManager::fileName() const { return m_fileName; }
QString FileManager::filePath() const { return m_filePath; }
bool FileManager::isLoaded() const { return m_loaded; }