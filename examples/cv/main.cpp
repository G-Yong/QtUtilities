#include "../../src/cv.h"

#include <QGuiApplication>
#include <QDebug>
#include <QFont>
#include <QPen>
#include <QColor>
#include <QImage>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // -----------------------------------------------------------------------
    // 1. createChessBoard — generate a checkerboard (15 rows, 20 cols, 32 px/cell)
    // -----------------------------------------------------------------------
    cv::Mat chess = QtUtil::createChessBoard(15, 20, 32);
    qInfo() << "[createChessBoard] size:" << chess.cols << "x" << chess.rows
            << "  channels:" << chess.channels();

    // -----------------------------------------------------------------------
    // 2. drawText — paint Unicode / Chinese text directly on the Mat
    // -----------------------------------------------------------------------
    QFont font("Arial", 28, QFont::Bold);
    QPen  pen{QColor(Qt::red)};
    int ret = QtUtil::drawText(chess, "Hello 你好 OpenCV", QPoint(20, 60), font, pen);
    qInfo() << "[drawText] returned:" << ret;

    // -----------------------------------------------------------------------
    // 3. saveMat — write to disk using Qt I/O (supports Chinese paths)
    // -----------------------------------------------------------------------
    ret = QtUtil::saveMat("output_chess.png", chess);
    qInfo() << "[saveMat] returned:" << ret
            << " -> output_chess.png";

    // -----------------------------------------------------------------------
    // 4. loadMat — read back using Qt I/O (supports Chinese paths)
    // -----------------------------------------------------------------------
    cv::Mat loaded = QtUtil::loadMat("output_chess.png");
    if (loaded.empty()) {
        qWarning() << "[loadMat] failed to load output_chess.png";
    } else {
        qInfo() << "[loadMat] size:" << loaded.cols << "x" << loaded.rows;
    }

    // -----------------------------------------------------------------------
    // 5. matToImage / imageToMat — convert cv::Mat <-> QImage
    // -----------------------------------------------------------------------
    QImage qimg = QtUtil::matToImage(loaded);
    qInfo() << "[matToImage] size:" << qimg.width() << "x" << qimg.height()
            << "  format:" << qimg.format();

    cv::Mat roundTrip = QtUtil::imageToMat(qimg);
    qInfo() << "[imageToMat] size:" << roundTrip.cols << "x" << roundTrip.rows
            << "  channels:" << roundTrip.channels();

    // -----------------------------------------------------------------------
    // 6. matToBytes / bytesToMat — encode and decode image bytes
    // -----------------------------------------------------------------------
    QByteArray jpegBytes = QtUtil::matToBytes(chess, "jpg");
    qInfo() << "[matToBytes] JPEG size:" << jpegBytes.size() << "bytes";

    cv::Mat decodedJpeg = QtUtil::bytesToMat(jpegBytes);
    qInfo() << "[bytesToMat] JPEG size:" << decodedJpeg.cols << "x" << decodedJpeg.rows;

    QByteArray pngBytes = QtUtil::matToBytes(chess, "png");
    qInfo() << "[matToBytes] PNG  size:" << pngBytes.size() << "bytes";

    // -----------------------------------------------------------------------
    // 7. imageToBytes / bytesToImage — encode/decode QImage directly
    // -----------------------------------------------------------------------
    QImage chessImage = QtUtil::matToImage(QtUtil::createChessBoard(10, 13, 24,
                                                                    QColor(80, 30, 30),
                                                                    QColor(200, 160, 120)));
    QByteArray imageBytes = QtUtil::imageToBytes(chessImage, "png");
    QImage decodedImage = QtUtil::bytesToImage(imageBytes);
    qInfo() << "[imageToBytes/bytesToImage] PNG size:" << imageBytes.size()
            << " decoded:" << decodedImage.width() << "x" << decodedImage.height();

    qInfo() << "All done.";
    return 0;
}
