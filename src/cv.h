/*
 * cv.h — OpenCV + Qt interoperability utilities (single-file header library, C++17 inline style)
 *
 * Requires C++17 or later (for inline variables/functions).
 * Requires Qt and OpenCV 4.x.
 *
 * Add to your .pro:
 *   QT       += core gui
 *   CONFIG   += c++17
 *   INCLUDEPATH += /path/to/opencv/include
 *   CONFIG(debug, debug|release) {
 *       LIBS += -L/path/to/opencv/x64/vc15/lib -lopencv_world410d
 *   } else {
 *       LIBS += -L/path/to/opencv/x64/vc15/lib -lopencv_world410
 *   }
 *
 * USAGE
 * -----
 * Just #include this header anywhere you need it — no .cpp compilation required.
 *
 * QUICK START
 * -----------
 *   #include "cv.h"
 *
 *   // Create a checkerboard pattern (15 rows, 20 cols, 32 px/cell) and save it
 *   cv::Mat chess = QtUtil::createChessBoard(15, 20, 32);
 *   QtUtil::saveMat("chess.png", chess);
 *
 *   // Load an image from a path that may contain Chinese characters
 *   cv::Mat img = QtUtil::loadMat("图片/test.jpg");
 *
 *   // Draw Chinese text directly on a cv::Mat
 *   QFont  font("SimHei", 24);
 *   QPen   pen(Qt::red);
 *   QtUtil::drawText(img, "Hello 你好", QPoint(10, 40), font, pen);
 *
 *   // Convert between cv::Mat and QImage
 *   QImage qimg = QtUtil::matToImage(img);
 *   cv::Mat mat = QtUtil::imageToMat(qimg);
 *
 *   // Encode a Mat to JPEG bytes (e.g. for network transmission)
 *   QByteArray bytes = QtUtil::matToBytes(img, "jpg");
 *   cv::Mat decoded = QtUtil::bytesToMat(bytes);
 *
 * FUNCTIONS
 * ---------
 *   QImage     matToImage    (const cv::Mat &mat)
 *   cv::Mat    imageToMat    (const QImage &image)
 *   QByteArray matToBytes    (const cv::Mat &mat, const QString &format,
 *                             const std::vector<int> &params = {})
 *   cv::Mat    bytesToMat    (const QByteArray &bytes, int flags = cv::IMREAD_UNCHANGED)
 *   QByteArray imageToBytes  (const QImage &image, const QString &format, int quality = -1)
 *   QImage     bytesToImage  (const QByteArray &bytes)
 *   cv::Mat    loadMat       (const QString &imgPath, int flags = cv::IMREAD_UNCHANGED)
 *   int        saveMat       (const QString &imgPath, const cv::Mat &mat)
 *   int        drawText      (cv::Mat &img, const QString &text, QPoint org,
 *                             const QFont &font, const QPen &pen)
 *   cv::Mat    createChessBoard(int rows, int cols, int cellSize,
 *                               const QColor &darkColor  = QColor(100, 100, 100),
 *                               const QColor &lightColor = QColor(150, 150, 150))
 */

#ifndef QTUTIL_CV_H
#define QTUTIL_CV_H

#include <opencv2/opencv.hpp>

#include <QImage>
#include <QColor>
#include <QFont>
#include <QPen>
#include <QPoint>
#include <QPainter>
#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <QBuffer>
#include <QIODevice>
#include <QString>

#include <algorithm>
#include <vector>

namespace QtUtil {

namespace detail {

inline QString imageExtension(QString format)
{
    format = format.trimmed();
    if (format.isEmpty())
        return QString();
    return format.startsWith('.') ? format : ("." + format);
}

inline QByteArray imageFormatName(QString format)
{
    if (format.startsWith('.'))
        format.remove(0, 1);
    return format.trimmed().toLatin1();
}

} // namespace detail

// ---------------------------------------------------------------------------
// matToImage
// Convert a cv::Mat (CV_8UC1 / CV_8UC3 / CV_8UC4) to a QImage.
// The returned QImage is a deep copy; the Mat's lifetime does not need to
// exceed that of the QImage.
// Returns a null QImage if the format is unsupported or the mat is empty.
// ---------------------------------------------------------------------------
inline QImage matToImage(const cv::Mat &mat)
{
    if (mat.empty())
        return QImage();

    switch (mat.type()) {
    case CV_8UC1:
        return QImage(mat.data, mat.cols, mat.rows,
                      static_cast<int>(mat.step),
                      QImage::Format_Grayscale8).copy();

    case CV_8UC3:
        // OpenCV stores BGR; QImage::Format_RGB888 expects RGB.
        // We convert so that colours are correct.
        {
            cv::Mat rgb;
            cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
            return QImage(rgb.data, rgb.cols, rgb.rows,
                          static_cast<int>(rgb.step),
                          QImage::Format_RGB888).copy();
        }

    case CV_8UC4:
        // OpenCV stores BGRA; QImage::Format_ARGB32 expects ARGB on big-endian
        // and BGRA on little-endian (x86).  Use Format_RGBA8888 which always
        // matches the RGBA byte order expected by Qt on all platforms when the
        // source is RGBA. OpenCV's BGRA can be remapped to RGBA first:
        {
            cv::Mat rgba;
            cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
            return QImage(rgba.data, rgba.cols, rgba.rows,
                          static_cast<int>(rgba.step),
                          QImage::Format_RGBA8888).copy();
        }

    default:
        return QImage();
    }
}

// ---------------------------------------------------------------------------
// imageToMat
// Convert a QImage to a cv::Mat using OpenCV's channel order.
// Returned Mat is a deep copy and does not depend on the QImage lifetime.
// Grayscale images become CV_8UC1, RGB-like images become CV_8UC3 (BGR), and
// alpha images become CV_8UC4 (BGRA).
// ---------------------------------------------------------------------------
inline cv::Mat imageToMat(const QImage &image)
{
    if (image.isNull())
        return cv::Mat();

    switch (image.format()) {
    case QImage::Format_Grayscale8:
    case QImage::Format_Indexed8:
        {
            QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
            return cv::Mat(gray.height(), gray.width(), CV_8UC1,
                           const_cast<uchar *>(gray.constBits()),
                           static_cast<size_t>(gray.bytesPerLine())).clone();
        }

    case QImage::Format_RGB888:
        {
            QImage rgb = image.convertToFormat(QImage::Format_RGB888);
            cv::Mat rgbMat(rgb.height(), rgb.width(), CV_8UC3,
                           const_cast<uchar *>(rgb.constBits()),
                           static_cast<size_t>(rgb.bytesPerLine()));
            cv::Mat bgr;
            cv::cvtColor(rgbMat, bgr, cv::COLOR_RGB2BGR);
            return bgr;
        }

    case QImage::Format_RGBA8888:
        {
            QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
            cv::Mat rgbaMat(rgba.height(), rgba.width(), CV_8UC4,
                            const_cast<uchar *>(rgba.constBits()),
                            static_cast<size_t>(rgba.bytesPerLine()));
            cv::Mat bgra;
            cv::cvtColor(rgbaMat, bgra, cv::COLOR_RGBA2BGRA);
            return bgra;
        }

    default:
        if (image.hasAlphaChannel()) {
            QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
            cv::Mat rgbaMat(rgba.height(), rgba.width(), CV_8UC4,
                            const_cast<uchar *>(rgba.constBits()),
                            static_cast<size_t>(rgba.bytesPerLine()));
            cv::Mat bgra;
            cv::cvtColor(rgbaMat, bgra, cv::COLOR_RGBA2BGRA);
            return bgra;
        }

        QImage rgb = image.convertToFormat(QImage::Format_RGB888);
        cv::Mat rgbMat(rgb.height(), rgb.width(), CV_8UC3,
                       const_cast<uchar *>(rgb.constBits()),
                       static_cast<size_t>(rgb.bytesPerLine()));
        cv::Mat bgr;
        cv::cvtColor(rgbMat, bgr, cv::COLOR_RGB2BGR);
        return bgr;
    }
}

// ---------------------------------------------------------------------------
// matToBytes
// Encode a cv::Mat to a QByteArray in the specified image format.
// format — file extension with or without the leading dot, e.g. "jpg", ".png".
// The Mat must be in BGR (or BGRA / grayscale) format as produced by OpenCV.
// Returns an empty QByteArray on failure.
// ---------------------------------------------------------------------------
inline QByteArray matToBytes(const cv::Mat &mat,
                             const QString &format,
                             const std::vector<int> &params = std::vector<int>())
{
    if (mat.empty())
        return QByteArray();

    QString ext = detail::imageExtension(format);
    if (ext.isEmpty())
        return QByteArray();

    std::vector<uchar> buf;
    if (!cv::imencode(ext.toStdString(), mat, buf, params))
        return QByteArray();

    return QByteArray(reinterpret_cast<const char *>(buf.data()),
                      static_cast<int>(buf.size()));
}

// ---------------------------------------------------------------------------
// bytesToMat
// Decode image bytes into a cv::Mat. The default keeps the original channels
// and bit depth, matching cv::imdecode(..., cv::IMREAD_UNCHANGED).
// ---------------------------------------------------------------------------
inline cv::Mat bytesToMat(const QByteArray &bytes, int flags = cv::IMREAD_UNCHANGED)
{
    if (bytes.isEmpty())
        return cv::Mat();

    std::vector<uchar> buf(bytes.begin(), bytes.end());
    return cv::imdecode(buf, flags);
}

// ---------------------------------------------------------------------------
// imageToBytes / bytesToImage
// Encode and decode QImage through Qt's image plugins. The format parameter
// accepts values like "png", "jpg", or ".bmp". quality follows QImage::save:
// -1 means Qt default, otherwise 0..100 where supported by the format.
// ---------------------------------------------------------------------------
inline QByteArray imageToBytes(const QImage &image,
                               const QString &format,
                               int quality = -1)
{
    if (image.isNull())
        return QByteArray();

    QByteArray formatName = detail::imageFormatName(format);
    if (formatName.isEmpty())
        return QByteArray();

    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly))
        return QByteArray();

    if (!image.save(&buffer, formatName.constData(), quality))
        return QByteArray();

    return bytes;
}

inline QImage bytesToImage(const QByteArray &bytes)
{
    QImage image;
    image.loadFromData(bytes);
    return image;
}

// ---------------------------------------------------------------------------
// drawText
// Draw a text string (including Unicode / Chinese characters) directly on a
// cv::Mat using Qt's font-rendering pipeline.  The Mat is modified in-place.
// Supported Mat types: CV_8UC1, CV_8UC3, CV_8UC4.
// Returns 0 on success, -1 if the channel count is unsupported.
// ---------------------------------------------------------------------------
inline int drawText(cv::Mat &img,
                    const QString &text,
                    QPoint org,
                    const QFont &font,
                    const QPen &pen)
{
    QImage tmp = matToImage(img);
    if (tmp.isNull())
        return -1;

    QPainter painter(&tmp);
    painter.setPen(pen);
    painter.setFont(font);
    painter.drawText(org, text);
    painter.end();

    img = imageToMat(tmp);

    return 0;
}

// ---------------------------------------------------------------------------
// loadMat
// Load an image from imgPath into a cv::Mat.
// Uses Qt's file I/O to read the raw bytes and cv::imdecode to decode them,
// which correctly handles file paths that contain Unicode / Chinese characters
// (unlike cv::imread which uses the C runtime's fopen and can fail on such
// paths on Windows).
// Returns an empty Mat on failure.
// ---------------------------------------------------------------------------
inline cv::Mat loadMat(const QString &imgPath, int flags = cv::IMREAD_UNCHANGED)
{
    QFile file(imgPath);
    if (!file.exists() || !file.open(QFile::ReadOnly))
        return cv::Mat();

    return bytesToMat(file.readAll(), flags);
}

// ---------------------------------------------------------------------------
// saveMat
// Save a cv::Mat to imgPath.
// Like loadMat, uses Qt's file I/O so that paths with Unicode / Chinese
// characters are handled correctly on Windows.
// Returns 0 on success, -1 on failure.
// ---------------------------------------------------------------------------
inline int saveMat(const QString &imgPath, const cv::Mat &mat)
{
    QFileInfo info(imgPath);
    QString ext = detail::imageExtension(info.suffix());
    if (mat.empty() || ext.isEmpty())
        return -1;

    QByteArray bytes = matToBytes(mat, ext);
    if (bytes.isEmpty())
        return -1;

    QFile file(imgPath);
    if (!file.open(QFile::WriteOnly | QFile::Truncate))
        return -1;

    if (file.write(bytes) != bytes.size())
        return -1;

    return 0;
}

// ---------------------------------------------------------------------------
// createChessBoard
// Generate a checkerboard pattern as a BGR cv::Mat.
// rows / cols — number of cell rows and columns.
// cellSize    — side length of each cell in pixels.
// darkColor   — colour of the "black" cells  (default: dark grey).
// lightColor  — colour of the "white" cells  (default: light grey).
// ---------------------------------------------------------------------------
inline cv::Mat createChessBoard(int rows, int cols, int cellSize,
                                const QColor &darkColor  = QColor(100, 100, 100),
                                const QColor &lightColor = QColor(150, 150, 150))
{
    using namespace cv;
    if (rows <= 0 || cols <= 0 || cellSize <= 0)
        return Mat();

    const Scalar dark (darkColor.blue(),  darkColor.green(),  darkColor.red());
    const Scalar light(lightColor.blue(), lightColor.green(), lightColor.red());

    Mat board(rows * cellSize, cols * cellSize, CV_8UC3, dark);

    for (int r = 0; r < rows; ++r) {
        for (int c = r % 2; c < cols; c += 2) {
            rectangle(board,
                      Rect(c * cellSize, r * cellSize, cellSize, cellSize),
                      light, -1);
        }
    }

    return board;
}

} // namespace QtUtil

#endif // QTUTIL_CV_H
