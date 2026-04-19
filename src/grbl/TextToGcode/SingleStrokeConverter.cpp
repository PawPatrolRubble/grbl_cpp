#include "SingleStrokeConverter.h"

#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetricsF>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cstdint>
#include <set>
#include <vector>

// ---------------------------------------------------------------------------
// Zhang-Suen thinning (no ximgproc dependency)
// ---------------------------------------------------------------------------

namespace {

/// One sub-iteration of the Zhang-Suen algorithm.
/// \p phase: 0 = first sub-iteration, 1 = second.
/// Returns number of pixels removed.
int zhangSuenStep(cv::Mat &img, int phase)
{
    const int rows = img.rows;
    const int cols = img.cols;

    // Mark pixels for deletion (cannot modify while iterating)
    std::vector<cv::Point> toRemove;
    toRemove.reserve(256);

    for (int r = 1; r < rows - 1; ++r) {
        const uint8_t *prev = img.ptr<uint8_t>(r - 1);
        const uint8_t *curr = img.ptr<uint8_t>(r);
        const uint8_t *next = img.ptr<uint8_t>(r + 1);
        for (int c = 1; c < cols - 1; ++c) {
            if (curr[c] == 0) continue;  // background

            // 8-neighbors (P2..P9 clockwise from top)
            int p2 = prev[c]     ? 1 : 0;
            int p3 = prev[c + 1] ? 1 : 0;
            int p4 = curr[c + 1] ? 1 : 0;
            int p5 = next[c + 1] ? 1 : 0;
            int p6 = next[c]     ? 1 : 0;
            int p7 = next[c - 1] ? 1 : 0;
            int p8 = curr[c - 1] ? 1 : 0;
            int p9 = prev[c - 1] ? 1 : 0;

            int B = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;
            if (B < 2 || B > 6) continue;

            // A = transitions 0→1 in the ordered sequence P2,P3,...,P9,P2
            int n[9] = {p2, p3, p4, p5, p6, p7, p8, p9, p2};
            int A = 0;
            for (int i = 0; i < 8; ++i)
                if (n[i] == 0 && n[i + 1] == 1) ++A;
            if (A != 1) continue;

            if (phase == 0) {
                // Condition 3: P2 * P4 * P6 == 0
                // Condition 4: P4 * P6 * P8 == 0
                if (p2 * p4 * p6 != 0) continue;
                if (p4 * p6 * p8 != 0) continue;
            } else {
                // Condition 3': P2 * P4 * P8 == 0
                // Condition 4': P2 * P6 * P8 == 0
                if (p2 * p4 * p8 != 0) continue;
                if (p2 * p6 * p8 != 0) continue;
            }

            toRemove.emplace_back(c, r);
        }
    }

    for (const auto &pt : toRemove)
        img.at<uint8_t>(pt.y, pt.x) = 0;

    return static_cast<int>(toRemove.size());
}

/// Apply full Zhang-Suen thinning until convergence.
void zhangSuenThinning(cv::Mat &binary)
{
    // Ensure single-channel uint8
    CV_Assert(binary.type() == CV_8UC1);

    // Normalize to 0/1
    cv::threshold(binary, binary, 128, 1, cv::THRESH_BINARY);

    for (;;) {
        int removed = zhangSuenStep(binary, 0);
        removed    += zhangSuenStep(binary, 1);
        if (removed == 0) break;
    }

    binary *= 255;  // back to 0/255
}

// ---------------------------------------------------------------------------
// Skeleton tracing → polylines
// ---------------------------------------------------------------------------

static constexpr int dx8[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
static constexpr int dy8[8] = {-1,  0,  1,-1, 1,-1, 0, 1};

int neighborCount(const cv::Mat &skel, int r, int c)
{
    int count = 0;
    for (int i = 0; i < 8; ++i) {
        int nr = r + dx8[i], nc = c + dy8[i];
        if (nr >= 0 && nr < skel.rows && nc >= 0 && nc < skel.cols
            && skel.at<uint8_t>(nr, nc))
            ++count;
    }
    return count;
}

/// Iteratively prune short spur branches from the skeleton.
/// Removes endpoint pixels whose chain length is below the threshold.
void pruneSkeleton(cv::Mat &skel, int pruneLen)
{
    if (pruneLen < 2) return;

    bool changed = true;
    while (changed) {
        changed = false;
        // Find all current endpoints
        std::vector<cv::Point> endpoints;
        for (int r = 1; r < skel.rows - 1; ++r) {
            for (int c = 1; c < skel.cols - 1; ++c) {
                if (skel.at<uint8_t>(r, c) && neighborCount(skel, r, c) == 1)
                    endpoints.emplace_back(c, r);
            }
        }

        for (const auto &ep : endpoints) {
            // Walk from endpoint, collecting pixels, until we hit a junction
            // (degree >= 3) or reach pruneLen pixels.
            std::vector<cv::Point> chain;
            int cr = ep.y, cc = ep.x;
            // Temporarily mark visited during this walk
            cv::Mat visited = cv::Mat::zeros(skel.size(), CV_8UC1);
            bool isShort = true;
            while (true) {
                if (skel.at<uint8_t>(cr, cc) == 0) break;
                visited.at<uint8_t>(cr, cc) = 1;
                chain.emplace_back(cc, cr);
                if (static_cast<int>(chain.size()) >= pruneLen) {
                    isShort = false;
                    break;
                }
                // Count unvisited neighbors
                int nextR = -1, nextC = -1;
                int nCount = 0;
                for (int i = 0; i < 8; ++i) {
                    int nr = cr + dx8[i], nc = cc + dy8[i];
                    if (nr >= 0 && nr < skel.rows && nc >= 0 && nc < skel.cols
                        && skel.at<uint8_t>(nr, nc) && !visited.at<uint8_t>(nr, nc)) {
                        ++nCount;
                        nextR = nr;
                        nextC = nc;
                    }
                }
                if (nCount == 0) break;       // dead end (isolated spur)
                if (nCount >= 2) break;        // junction — stop, this is a real branch
                cr = nextR;
                cc = nextC;
            }

            if (isShort && !chain.empty()) {
                // Remove the short spur
                for (const auto &pt : chain)
                    skel.at<uint8_t>(pt.y, pt.x) = 0;
                changed = true;
            }
        }
    }
}

/// Trace one chain starting from (r,c). Sets visited pixels.
std::vector<cv::Point2f> traceChain(const cv::Mat &skel, cv::Mat &visited, int r0, int c0)
{
    std::vector<cv::Point2f> chain;
    int r = r0, c = c0;
    visited.at<uint8_t>(r, c) = 1;
    chain.emplace_back(static_cast<float>(c), static_cast<float>(r));

    for (;;) {
        bool found = false;
        for (int i = 0; i < 8; ++i) {
            int nr = r + dx8[i], nc = c + dy8[i];
            if (nr >= 0 && nr < skel.rows && nc >= 0 && nc < skel.cols
                && skel.at<uint8_t>(nr, nc) && !visited.at<uint8_t>(nr, nc)) {
                visited.at<uint8_t>(nr, nc) = 1;
                chain.emplace_back(static_cast<float>(nc), static_cast<float>(nr));
                r = nr;
                c = nc;
                found = true;
                break;
            }
        }
        if (!found) break;
    }
    return chain;
}

/// Trace entire skeleton into polyline chains.
std::vector<std::vector<cv::Point2f>> traceSkeleton(const cv::Mat &skel)
{
    cv::Mat visited = cv::Mat::zeros(skel.size(), CV_8UC1);
    std::vector<std::vector<cv::Point2f>> chains;

    // First pass: start from endpoints (degree == 1) for cleaner paths
    for (int r = 1; r < skel.rows - 1; ++r) {
        for (int c = 1; c < skel.cols - 1; ++c) {
            if (skel.at<uint8_t>(r, c) && !visited.at<uint8_t>(r, c)
                && neighborCount(skel, r, c) == 1) {
                auto chain = traceChain(skel, visited, r, c);
                if (chain.size() >= 2)
                    chains.push_back(std::move(chain));
            }
        }
    }

    // Second pass: remaining (loop/junction) pixels
    for (int r = 1; r < skel.rows - 1; ++r) {
        for (int c = 1; c < skel.cols - 1; ++c) {
            if (skel.at<uint8_t>(r, c) && !visited.at<uint8_t>(r, c)) {
                auto chain = traceChain(skel, visited, r, c);
                if (chain.size() >= 2)
                    chains.push_back(std::move(chain));
            }
        }
    }

    return chains;
}

/// Simplify chains with Ramer-Douglas-Peucker.
void simplifyChains(std::vector<std::vector<cv::Point2f>> &chains, double epsilon)
{
    for (auto &chain : chains) {
        if (chain.size() < 3) continue;
        std::vector<cv::Point2f> simplified;
        cv::approxPolyDP(chain, simplified, epsilon, false);
        chain = std::move(simplified);
    }
}

/// Greedy nearest-neighbor stitching: merge chains whose endpoints
/// are within `maxGap` pixels. Dramatically reduces pen-lifts.
void mergeChains(std::vector<std::vector<cv::Point2f>> &chains, double maxGap)
{
    if (maxGap <= 0.0 || chains.size() < 2) return;

    double gapSq = maxGap * maxGap;
    std::vector<bool> used(chains.size(), false);
    std::vector<std::vector<cv::Point2f>> merged;

    auto distSq = [](const cv::Point2f &a, const cv::Point2f &b) {
        float dx = a.x - b.x, dy = a.y - b.y;
        return static_cast<double>(dx * dx + dy * dy);
    };

    for (size_t start = 0; start < chains.size(); ++start) {
        if (used[start]) continue;
        used[start] = true;

        // Build a combined path by repeatedly appending the nearest chain
        std::vector<cv::Point2f> current = chains[start];

        bool found = true;
        while (found) {
            found = false;
            double bestDist = gapSq;
            size_t bestIdx = 0;
            int bestEnd = 0;  // 0 = front→front, 1 = front→back, 2 = back→front, 3 = back→back

            const cv::Point2f &curFront = current.front();
            const cv::Point2f &curBack  = current.back();

            for (size_t i = 0; i < chains.size(); ++i) {
                if (used[i]) continue;
                const cv::Point2f &cF = chains[i].front();
                const cv::Point2f &cB = chains[i].back();

                // Check all 4 endpoint combinations
                double d;
                d = distSq(curBack, cF);
                if (d < bestDist) { bestDist = d; bestIdx = i; bestEnd = 0; found = true; }
                d = distSq(curBack, cB);
                if (d < bestDist) { bestDist = d; bestIdx = i; bestEnd = 1; found = true; }
                d = distSq(curFront, cF);
                if (d < bestDist) { bestDist = d; bestIdx = i; bestEnd = 2; found = true; }
                d = distSq(curFront, cB);
                if (d < bestDist) { bestDist = d; bestIdx = i; bestEnd = 3; found = true; }
            }

            if (found) {
                used[bestIdx] = true;
                auto &other = chains[bestIdx];

                switch (bestEnd) {
                case 0:  // append other (as-is) to end of current
                    current.insert(current.end(), other.begin(), other.end());
                    break;
                case 1:  // append reversed other to end
                    current.insert(current.end(), other.rbegin(), other.rend());
                    break;
                case 2:  // prepend reversed other to front
                    std::reverse(other.begin(), other.end());
                    other.insert(other.end(), current.begin(), current.end());
                    current = std::move(other);
                    break;
                case 3:  // prepend other (as-is) to front
                    other.insert(other.end(), current.begin(), current.end());
                    current = std::move(other);
                    break;
                }
            }
        }

        if (current.size() >= 2)
            merged.push_back(std::move(current));
    }

    chains = std::move(merged);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

QList<QPolygonF> SingleStrokeConverter::convert(
    const QString &text,
    const QFont &font,
    double lineSpacing,
    const Options &opts)
{
    if (text.isEmpty()) return {};

    // ---- Build the same QPainterPath as TextEntity uses ----
    QPainterPath fullPath;
    QStringList lines = text.split('\n');
    QFont renderFont(font);
    // We'll compute the transform to map pixel coords back to path coords

    // Metrics in path-space
    QFontMetricsF fm(font);
    double lineHeight = fm.height() * lineSpacing;

    QList<QPolygonF> allStrokes;

    // Process each character individually for better skeleton quality
    double cursorX = 0.0;
    double cursorY = 0.0;

    for (int li = 0; li < lines.size(); ++li) {
        const QString &line = lines[li];
        cursorX = 0.0;

        for (int ci = 0; ci < line.size(); ++ci) {
            QChar ch = line[ci];
            if (ch == ' ') {
                cursorX += fm.horizontalAdvance(' ');
                continue;
            }

            // Get bounding box of this character in path space
            QPainterPath charPath;
            charPath.addText(0, 0, font, QString(ch));
            QRectF pathBounds = charPath.boundingRect();
            if (pathBounds.isEmpty()) {
                cursorX += fm.horizontalAdvance(ch);
                continue;
            }

            // Determine render size: map path extents into pixel space
            double scaleX = (opts.glyphSize - 10) / std::max(pathBounds.width(), 1.0);
            double scaleY = (opts.glyphSize - 10) / std::max(pathBounds.height(), 1.0);
            double scale  = std::min(scaleX, scaleY);

            int imgW = std::max(16, static_cast<int>(pathBounds.width() * scale + 10));
            int imgH = std::max(16, static_cast<int>(pathBounds.height() * scale + 10));

            // Render glyph as white-on-black bitmap
            QImage img(imgW, imgH, QImage::Format_Grayscale8);
            img.fill(0);
            {
                QPainter painter(&img);
                painter.setRenderHint(QPainter::Antialiasing, false);
                painter.setPen(Qt::NoPen);
                painter.setBrush(Qt::white);

                // Transform: shift so glyph is centered in image
                QPainterPath scaled;
                QTransform xform;
                xform.translate(5.0 - pathBounds.left() * scale,
                                5.0 - pathBounds.top() * scale);
                xform.scale(scale, scale);
                scaled = xform.map(charPath);
                painter.drawPath(scaled);
            }

            // Convert QImage → cv::Mat
            cv::Mat binary(imgH, imgW, CV_8UC1, img.bits(),
                           static_cast<size_t>(img.bytesPerLine()));
            cv::Mat work;
            binary.copyTo(work);  // deep copy (QImage owns the original buffer)

            // Threshold to clean binary
            cv::threshold(work, work, 128, 255, cv::THRESH_BINARY);

            // Optional morphological erosion to strip serif decorations
            // and thin-out wide strokes for cleaner skeleton
            int erode = opts.erodePixels;
            if (erode <= 0) {
                // Auto: estimate average stroke width from binary coverage
                int fgCount = cv::countNonZero(work);
                double coverageRatio = static_cast<double>(fgCount) / (imgW * imgH);
                // Higher coverage → thicker strokes → more erosion
                erode = (coverageRatio > 0.35) ? 2 : (coverageRatio > 0.2) ? 1 : 0;
            }
            if (erode > 0) {
                cv::Mat kernel = cv::getStructuringElement(
                    cv::MORPH_ELLIPSE, cv::Size(2 * erode + 1, 2 * erode + 1));
                cv::erode(work, work, kernel);
                // Re-dilate slightly to restore connectivity
                if (erode > 1) {
                    cv::Mat smallKernel = cv::getStructuringElement(
                        cv::MORPH_ELLIPSE, cv::Size(3, 3));
                    cv::dilate(work, work, smallKernel);
                }
            }

            // Thinning
            zhangSuenThinning(work);

            // Prune short spur branches
            pruneSkeleton(work, opts.pruneLength);

            // Trace, merge nearby endpoints, simplify
            auto chains = traceSkeleton(work);
            mergeChains(chains, opts.mergeGap);
            simplifyChains(chains, opts.epsilon);

            // Remove short segments
            if (opts.minSegLen > 0.0) {
                chains.erase(
                    std::remove_if(chains.begin(), chains.end(),
                        [&](const std::vector<cv::Point2f> &ch) {
                            double len = 0.0;
                            for (size_t i = 1; i < ch.size(); ++i) {
                                float dx = ch[i].x - ch[i-1].x;
                                float dy = ch[i].y - ch[i-1].y;
                                len += std::sqrt(dx*dx + dy*dy);
                            }
                            return len < opts.minSegLen;
                        }),
                    chains.end());
            }

            // Map pixel coords back to path coordinate space, offset by cursor
            double invScale = 1.0 / scale;
            for (const auto &chain : chains) {
                QPolygonF poly;
                poly.reserve(static_cast<int>(chain.size()));
                for (const auto &pt : chain) {
                    double px = (pt.x - 5.0) * invScale + pathBounds.left();
                    double py = (pt.y - 5.0) * invScale + pathBounds.top();
                    poly << QPointF(cursorX + px, cursorY + py);
                }
                if (poly.size() >= 2)
                    allStrokes.append(poly);
            }

            cursorX += fm.horizontalAdvance(ch);
        }
        cursorY += lineHeight;
    }

    return allStrokes;
}
