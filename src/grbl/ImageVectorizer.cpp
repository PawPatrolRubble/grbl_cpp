#include "ImageVectorizer.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <QFile>
#include <QDebug>
#include <cmath>
#include <algorithm>
#include <set>
#include <stack>

// ─── Image loading ──────────────────────────────────────────────

bool ImageVectorizer::loadImage(const QString& filePath)
{
    cv::Mat img = cv::imread(filePath.toStdString(), cv::IMREAD_GRAYSCALE);
    if (img.empty())
        return false;

    m_width = img.cols;
    m_height = img.rows;
    m_imageData = QByteArray(reinterpret_cast<const char*>(img.data),
                             m_width * m_height);
    m_originalData = m_imageData;
    return true;
}

void ImageVectorizer::loadImage(const QImage& image)
{
    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    m_width = gray.width();
    m_height = gray.height();

    m_imageData.resize(m_width * m_height);
    for (int y = 0; y < m_height; ++y) {
        const uchar* scanLine = gray.constScanLine(y);
        memcpy(m_imageData.data() + y * m_width, scanLine, m_width);
    }
    m_originalData = m_imageData;
}

// ─── Full pipeline ──────────────────────────────────────────────

QList<QList<QPointF>> ImageVectorizer::process()
{
    if (m_imageData.isEmpty())
        return {};

    // Reset to original for re-processing
    m_imageData = m_originalData;

    // Step 1: Already grayscale from loading

    // Step 2: Binarize or edge detect
    if (m_settings.useEdgeDetection)
        edgeDetect();
    else
        binarize();

    // Step 3: Thin to skeleton (1px wide strokes)
    thin();

    // Step 4: Trace skeleton pixels into connected polylines
    auto paths = tracePaths();

    // Step 5: Simplify paths with OpenCV approxPolyDP for smooth curves
    paths = simplifyPaths(paths);

    // Step 4: Scale to target dimensions
    paths = scalePaths(paths);

    return paths;
}

void ImageVectorizer::processForPreview()
{
    if (m_imageData.isEmpty())
        return;

    // Reset to original image data
    m_imageData = m_originalData;

    if (m_settings.useEdgeDetection)
        edgeDetect();
    else
        binarize();

    thin();
}

QImage ImageVectorizer::previewImage() const
{
    if (m_imageData.isEmpty() || m_width <= 0 || m_height <= 0)
        return {};

    QImage img(m_width, m_height, QImage::Format_Grayscale8);
    for (int y = 0; y < m_height; ++y) {
        memcpy(img.scanLine(y),
               m_imageData.constData() + y * m_width, m_width);
    }
    return img;
}

// ─── Binarization ───────────────────────────────────────────────

void ImageVectorizer::binarize()
{
    cv::Mat src(m_height, m_width, CV_8UC1,
                reinterpret_cast<uchar*>(m_imageData.data()));

    if (m_settings.invertImage)
        cv::bitwise_not(src, src);

    cv::Mat binary;
    if (m_settings.adaptiveThreshold) {
        int blockSize = m_settings.adaptiveBlockSize;
        if (blockSize % 2 == 0) blockSize++;
        if (blockSize < 3) blockSize = 3;
        cv::adaptiveThreshold(src, binary, 255,
                              cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                              cv::THRESH_BINARY_INV,
                              blockSize, 5);
    } else {
        cv::threshold(src, binary, m_settings.threshold, 255,
                      cv::THRESH_BINARY_INV);
    }

    // Copy result back — strokes are 255 (white), background is 0 (black)
    m_imageData = QByteArray(reinterpret_cast<const char*>(binary.data),
                             m_width * m_height);
}

// ─── Edge detection (Canny) ─────────────────────────────────────

void ImageVectorizer::edgeDetect()
{
    cv::Mat src(m_height, m_width, CV_8UC1,
                reinterpret_cast<uchar*>(m_imageData.data()));

    if (m_settings.invertImage)
        cv::bitwise_not(src, src);

    // Gaussian blur to reduce noise
    cv::Mat blurred;
    int kSize = m_settings.gaussianBlur;
    if (kSize % 2 == 0) kSize++;
    if (kSize < 1) kSize = 1;
    if (kSize >= 3)
        cv::GaussianBlur(src, blurred, cv::Size(kSize, kSize), 0);
    else
        blurred = src;

    // Canny edge detection
    cv::Mat edges;
    cv::Canny(blurred, edges, m_settings.cannyLow, m_settings.cannyHigh);

    m_imageData = QByteArray(reinterpret_cast<const char*>(edges.data),
                             m_width * m_height);
}

// ─── Contour extraction (OpenCV findContours + approxPolyDP) ────

QList<QList<QPointF>> ImageVectorizer::extractContours()
{
    QList<QList<QPointF>> paths;

    cv::Mat binary(m_height, m_width, CV_8UC1,
                   reinterpret_cast<uchar*>(m_imageData.data()));

    // Find contours — RETR_LIST gets all contours, CHAIN_APPROX_NONE for full resolution
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary.clone(), contours, cv::RETR_LIST, cv::CHAIN_APPROX_NONE);

    double epsilon = m_settings.simplifyEpsilon;

    for (const auto& contour : contours) {
        if (contour.size() < 3) continue;

        // Approximate the contour with Douglas-Peucker
        std::vector<cv::Point> approx;
        cv::approxPolyDP(contour, approx, epsilon, true); // true = closed contour

        if (approx.size() < 2) continue;

        QList<QPointF> path;
        for (const auto& pt : approx) {
            path.append(QPointF(pt.x, pt.y));
        }
        // Close the contour
        if (path.first() != path.last())
            path.append(path.first());

        paths.append(path);
    }

    return paths;
}

// ─── Zhang-Suen thinning ────────────────────────────────────────

int ImageVectorizer::countNeighborTransitions(const uchar* img, int x, int y, int stride)
{
    // Count 0→1 transitions in the ordered sequence P2,P3,...,P9,P2
    //   P9 P2 P3
    //   P8 P1 P4
    //   P7 P6 P5
    int p2 = img[(y-1)*stride + x]   > 0 ? 1 : 0;
    int p3 = img[(y-1)*stride + x+1] > 0 ? 1 : 0;
    int p4 = img[y*stride + x+1]     > 0 ? 1 : 0;
    int p5 = img[(y+1)*stride + x+1] > 0 ? 1 : 0;
    int p6 = img[(y+1)*stride + x]   > 0 ? 1 : 0;
    int p7 = img[(y+1)*stride + x-1] > 0 ? 1 : 0;
    int p8 = img[y*stride + x-1]     > 0 ? 1 : 0;
    int p9 = img[(y-1)*stride + x-1] > 0 ? 1 : 0;

    int transitions = 0;
    if (p2 == 0 && p3 == 1) transitions++;
    if (p3 == 0 && p4 == 1) transitions++;
    if (p4 == 0 && p5 == 1) transitions++;
    if (p5 == 0 && p6 == 1) transitions++;
    if (p6 == 0 && p7 == 1) transitions++;
    if (p7 == 0 && p8 == 1) transitions++;
    if (p8 == 0 && p9 == 1) transitions++;
    if (p9 == 0 && p2 == 1) transitions++;
    return transitions;
}

int ImageVectorizer::countNonZeroNeighbors(const uchar* img, int x, int y, int stride)
{
    int count = 0;
    if (img[(y-1)*stride + x]   > 0) count++;  // P2
    if (img[(y-1)*stride + x+1] > 0) count++;  // P3
    if (img[y*stride + x+1]     > 0) count++;  // P4
    if (img[(y+1)*stride + x+1] > 0) count++;  // P5
    if (img[(y+1)*stride + x]   > 0) count++;  // P6
    if (img[(y+1)*stride + x-1] > 0) count++;  // P7
    if (img[y*stride + x-1]     > 0) count++;  // P8
    if (img[(y-1)*stride + x-1] > 0) count++;  // P9
    return count;
}

void ImageVectorizer::thin()
{
    uchar* img = reinterpret_cast<uchar*>(m_imageData.data());
    const int w = m_width;
    const int h = m_height;

    QByteArray marker(w * h, 0);
    bool changed = true;

    while (changed) {
        changed = false;

        // Sub-iteration 1
        marker.fill(0);
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                if (img[y*w + x] == 0) continue;

                int nn = countNonZeroNeighbors(img, x, y, w);
                int nt = countNeighborTransitions(img, x, y, w);

                if (nn < 2 || nn > 6) continue;
                if (nt != 1) continue;

                int p2 = img[(y-1)*w + x] > 0 ? 1 : 0;
                int p4 = img[y*w + x+1]   > 0 ? 1 : 0;
                int p6 = img[(y+1)*w + x] > 0 ? 1 : 0;
                int p8 = img[y*w + x-1]   > 0 ? 1 : 0;

                if (p2 * p4 * p6 != 0) continue;
                if (p4 * p6 * p8 != 0) continue;

                marker[y*w + x] = 1;
            }
        }
        for (int i = 0; i < w * h; ++i) {
            if (marker[i]) { img[i] = 0; changed = true; }
        }

        // Sub-iteration 2
        marker.fill(0);
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                if (img[y*w + x] == 0) continue;

                int nn = countNonZeroNeighbors(img, x, y, w);
                int nt = countNeighborTransitions(img, x, y, w);

                if (nn < 2 || nn > 6) continue;
                if (nt != 1) continue;

                int p2 = img[(y-1)*w + x] > 0 ? 1 : 0;
                int p4 = img[y*w + x+1]   > 0 ? 1 : 0;
                int p6 = img[(y+1)*w + x] > 0 ? 1 : 0;
                int p8 = img[y*w + x-1]   > 0 ? 1 : 0;

                if (p2 * p4 * p8 != 0) continue;
                if (p2 * p6 * p8 != 0) continue;

                marker[y*w + x] = 1;
            }
        }
        for (int i = 0; i < w * h; ++i) {
            if (marker[i]) { img[i] = 0; changed = true; }
        }
    }
}

// ─── Path tracing ───────────────────────────────────────────────

QList<QList<QPointF>> ImageVectorizer::tracePaths()
{
    QList<QList<QPointF>> paths;
    const int w = m_width;
    const int h = m_height;

    // Work on a copy so we can mark visited pixels
    QByteArray visited(w * h, 0);
    const uchar* img = reinterpret_cast<const uchar*>(m_imageData.constData());

    // 8-connectivity neighbor offsets
    static const int dx[] = {1, 1, 0, -1, -1, -1, 0, 1};
    static const int dy[] = {0, 1, 1, 1, 0, -1, -1, -1};

    auto isWhite = [&](int x, int y) -> bool {
        if (x < 0 || x >= w || y < 0 || y >= h) return false;
        return img[y*w + x] > 0;
    };

    auto neighborCount = [&](int x, int y) -> int {
        int count = 0;
        for (int d = 0; d < 8; ++d) {
            if (isWhite(x + dx[d], y + dy[d])) count++;
        }
        return count;
    };

    // Find all endpoints (1 neighbor) and junction points (3+ neighbors)
    // Start tracing from endpoints first, then from any remaining unvisited white pixels
    QList<QPoint> endpoints, junctions;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!isWhite(x, y)) continue;
            int nc = neighborCount(x, y);
            if (nc == 1) endpoints.append(QPoint(x, y));
            else if (nc >= 3) junctions.append(QPoint(x, y));
        }
    }

    // Trace a single path from a starting pixel
    auto traceSingle = [&](int startX, int startY) -> QList<QPointF> {
        QList<QPointF> path;
        int cx = startX, cy = startY;
        visited[cy*w + cx] = 1;
        path.append(QPointF(cx, cy));

        while (true) {
            int bestX = -1, bestY = -1;
            int bestPriority = -1;

            for (int d = 0; d < 8; ++d) {
                int nx = cx + dx[d];
                int ny = cy + dy[d];
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                if (!isWhite(nx, ny)) continue;
                if (visited[ny*w + nx]) continue;

                // Prefer cardinal directions (d=0,2,4,6) over diagonal
                int prio = (d % 2 == 0) ? 2 : 1;
                if (prio > bestPriority) {
                    bestPriority = prio;
                    bestX = nx;
                    bestY = ny;
                }
            }

            if (bestX < 0) break;  // No unvisited neighbor

            cx = bestX;
            cy = bestY;
            visited[cy*w + cx] = 1;
            path.append(QPointF(cx, cy));
        }

        return path;
    };

    // Trace from endpoints first (produces longer, cleaner paths)
    for (const auto& ep : endpoints) {
        if (visited[ep.y()*w + ep.x()]) continue;
        auto path = traceSingle(ep.x(), ep.y());
        if (path.size() >= 2)
            paths.append(path);
    }

    // Then trace from any remaining unvisited white pixels
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!isWhite(x, y) || visited[y*w + x]) continue;
            auto path = traceSingle(x, y);
            if (path.size() >= 2)
                paths.append(path);
        }
    }

    return paths;
}

// ─── Path simplification ────────────────────────────────────────

QList<QPointF> ImageVectorizer::rdpSimplify(const QList<QPointF>& points, double epsilon)
{
    if (points.size() <= 2)
        return points;

    // Find the point with the maximum distance from the line segment (first, last)
    double maxDist = 0;
    int maxIdx = 0;
    const QPointF& first = points.first();
    const QPointF& last = points.last();

    double lineLen = std::hypot(last.x() - first.x(), last.y() - first.y());

    for (int i = 1; i < points.size() - 1; ++i) {
        double dist;
        if (lineLen < 1e-10) {
            dist = std::hypot(points[i].x() - first.x(), points[i].y() - first.y());
        } else {
            // Perpendicular distance from point to line
            double t = ((points[i].x() - first.x()) * (last.x() - first.x()) +
                        (points[i].y() - first.y()) * (last.y() - first.y())) / (lineLen * lineLen);
            t = std::clamp(t, 0.0, 1.0);
            double projX = first.x() + t * (last.x() - first.x());
            double projY = first.y() + t * (last.y() - first.y());
            dist = std::hypot(points[i].x() - projX, points[i].y() - projY);
        }
        if (dist > maxDist) {
            maxDist = dist;
            maxIdx = i;
        }
    }

    if (maxDist > epsilon) {
        // Recursively simplify
        auto left = rdpSimplify(points.mid(0, maxIdx + 1), epsilon);
        auto right = rdpSimplify(points.mid(maxIdx), epsilon);
        left.removeLast();
        left.append(right);
        return left;
    } else {
        return {first, last};
    }
}

QList<QList<QPointF>> ImageVectorizer::simplifyPaths(const QList<QList<QPointF>>& paths)
{
    QList<QList<QPointF>> result;
    double epsilon = m_settings.simplifyEpsilon;

    for (const auto& path : paths) {
        if (path.size() < 2) continue;

        // Convert to cv::Point2f for approxPolyDP
        std::vector<cv::Point2f> cvPoints;
        cvPoints.reserve(path.size());
        for (const auto& pt : path)
            cvPoints.emplace_back(static_cast<float>(pt.x()), static_cast<float>(pt.y()));

        std::vector<cv::Point2f> simplified;
        cv::approxPolyDP(cvPoints, simplified, epsilon, false); // false = open curve

        if (simplified.size() >= 2) {
            QList<QPointF> simplePath;
            for (const auto& pt : simplified)
                simplePath.append(QPointF(pt.x, pt.y));
            result.append(simplePath);
        }
    }
    return result;
}

// ─── Scaling ────────────────────────────────────────────────────

QList<QList<QPointF>> ImageVectorizer::scalePaths(const QList<QList<QPointF>>& paths)
{
    if (m_width <= 0 || m_height <= 0)
        return paths;

    double scale = m_settings.targetWidth / m_width;

    QList<QList<QPointF>> result;
    for (const auto& path : paths) {
        QList<QPointF> scaled;
        for (const auto& pt : path) {
            // Flip Y (image Y is top-down, G-code Y is bottom-up)
            scaled.append(QPointF(pt.x() * scale,
                                  (m_height - pt.y()) * scale));
        }
        result.append(scaled);
    }
    return result;
}
