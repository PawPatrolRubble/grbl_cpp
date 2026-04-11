#pragma once

#include <QVector3D>

struct MachinePosition
{
    double x = 0.0, y = 0.0, z = 0.0;

    MachinePosition() = default;
    MachinePosition(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
    explicit MachinePosition(const QVector3D &v) : x(v.x()), y(v.y()), z(v.z_) {}

    QVector3D toVector3D() const { return QVector3D(x, y, z); }
};

struct StreamProgress
{
    int totalLines = 0;
    int linesSent = 0;
    int linesCompleted = 0;
    int errors = 0;

    double percent() const {
        if (totalLines == 0) return 0.0;
        return (static_cast<double>(linesCompleted) / totalLines) * 100.0;
    }
};
