#pragma once

#include <QList>
#include <QPointF>
#include <QString>
#include <QImage>

/// Converts raster images to vector polylines suitable for G-code generation.
/// Pipeline: load → grayscale → threshold → thin (Zhang-Suen) → trace paths → simplify.
class ImageVectorizer
{
public:
    struct Settings {
        int threshold = 128;          // Binarization threshold (0-255)
        bool adaptiveThreshold = true; // Use adaptive vs global threshold
        int adaptiveBlockSize = 15;   // Block size for adaptive threshold
        double simplifyEpsilon = 0.2; // RDP simplification tolerance (pixels)
        bool invertImage = false;     // Invert before processing (dark bg → light strokes)
        double targetWidth = 100.0;   // Target width in mm for output

        // Edge detection mode
        bool useEdgeDetection = false; // Use Canny edge detection instead of threshold
        int cannyLow = 50;            // Canny low threshold
        int cannyHigh = 150;          // Canny high threshold
        int gaussianBlur = 3;         // Gaussian blur kernel size before Canny
    };

    ImageVectorizer() = default;

    /// Set processing parameters.
    void setSettings(const Settings& settings) { m_settings = settings; }
    Settings settings() const { return m_settings; }

    /// Load image from file path. Returns false on failure.
    bool loadImage(const QString& filePath);

    /// Load image from QImage.
    void loadImage(const QImage& image);

    /// Run the full processing pipeline. Returns the extracted polylines.
    QList<QList<QPointF>> process();

    /// Get the processed binary image as a QImage (for preview).
    /// Call after process(), or call processForPreview() to run up to binarize/edge step only.
    QImage previewImage() const;

    /// Run pipeline up to binarize/edge + thin step (for preview, no path extraction).
    void processForPreview();

    /// Get image dimensions (pixels) after loading.
    int imageWidth() const { return m_width; }
    int imageHeight() const { return m_height; }

private:
    // Pipeline stages (operate on internal cv::Mat stored as raw bytes)
    void toGrayscale();
    void binarize();
    void edgeDetect();  // Canny edge detection
    void thin();       // Zhang-Suen thinning
    QList<QList<QPointF>> extractContours(); // OpenCV contour detection + approxPolyDP
    QList<QList<QPointF>> tracePaths();
    QList<QList<QPointF>> simplifyPaths(const QList<QList<QPointF>>& paths);
    QList<QList<QPointF>> scalePaths(const QList<QList<QPointF>>& paths);

    // Zhang-Suen thinning helpers
    static int countNeighborTransitions(const uchar* img, int x, int y, int stride);
    static int countNonZeroNeighbors(const uchar* img, int x, int y, int stride);

    // Ramer-Douglas-Peucker simplification
    static QList<QPointF> rdpSimplify(const QList<QPointF>& points, double epsilon);

    Settings m_settings;
    int m_width = 0;
    int m_height = 0;

    // Image data stored as grayscale byte arrays (avoid OpenCV in header)
    QByteArray m_imageData;      // Current processing buffer (grayscale, 1 byte/pixel)
    QByteArray m_originalData;   // Original grayscale data (for re-processing with new settings)
};
