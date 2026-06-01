/*
 * file.h — File & directory utilities (single-file header library, C++17 inline style)
 *
 * Requires C++17 or later (for inline variables/functions).
 * Requires only Qt Core — no extra Qt modules.
 *   QT     += core
 *   CONFIG += c++17
 *
 * USAGE
 * -----
 * Just #include this header anywhere you need it — no .cpp compilation required.
 * Every function lives in the QtUtil namespace. All paths accept Unicode /
 * Chinese characters and text I/O is UTF-8 by default.
 *
 * QUICK START
 * -----------
 *   #include "file.h"
 *
 *   // Text I/O (UTF-8)
 *   QtUtil::writeText("out/note.txt", "你好 world\n");
 *   QtUtil::appendText("out/note.txt", "second line\n");
 *   QString text = QtUtil::readText("out/note.txt");
 *
 *   // Binary I/O
 *   QtUtil::writeBytes("out/data.bin", QByteArray("\x01\x02", 2));
 *   QByteArray raw = QtUtil::readBytes("out/data.bin");
 *
 *   // Filesystem operations
 *   QtUtil::ensureDir("out/sub");
 *   QtUtil::copyFile("a.txt", "b.txt", true);     // overwrite = true
 *   QtUtil::moveFile("b.txt", "out/sub/b.txt");
 *   QtUtil::removeFile("a.txt");
 *
 *   // Listing
 *   QStringList pngs = QtUtil::listFiles("images", {"*.png", "*.jpg"}, true);
 *   QStringList dirs = QtUtil::listDirs("images");
 *
 *   // Misc
 *   qint64  size = QtUtil::fileSize("out/data.bin");
 *   QString hash = QtUtil::fileHashHex("out/data.bin");   // SHA-256 by default
 *
 * FUNCTIONS
 * ---------
 *   QString     readText        (const QString &path)
 *   QByteArray  readBytes       (const QString &path)
 *   bool        writeText       (const QString &path, const QString &text, bool append = false)
 *   bool        appendText      (const QString &path, const QString &text)
 *   bool        writeBytes      (const QString &path, const QByteArray &data, bool append = false)
 *   bool        exists          (const QString &path)
 *   bool        isFile          (const QString &path)
 *   bool        isDir           (const QString &path)
 *   qint64      fileSize        (const QString &path)
 *   bool        ensureDir       (const QString &dirPath)
 *   bool        copyFile        (const QString &src, const QString &dst, bool overwrite = false)
 *   bool        moveFile        (const QString &src, const QString &dst, bool overwrite = false)
 *   bool        removeFile      (const QString &path)
 *   bool        removeDir       (const QString &dirPath)
 *   QStringList listFiles       (const QString &dirPath, const QStringList &filters = {},
 *                                bool recursive = false)
 *   QStringList listDirs        (const QString &dirPath, bool recursive = false)
 *   QString     fileHashHex     (const QString &path,
 *                                QCryptographicHash::Algorithm algo = QCryptographicHash::Sha256)
 *   QString     baseName        (const QString &path)   // file name without suffix
 *   QString     fileName        (const QString &path)   // file name with suffix
 *   QString     suffix          (const QString &path)   // extension, no dot
 *   QString     parentDir       (const QString &path)
 *   QString     joinPath        (const QString &a, const QString &b)
 */

#ifndef QTUTIL_FILE_H
#define QTUTIL_FILE_H

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QDirIterator>

namespace QtUtil {

// ---------------------------------------------------------------------------
// readText / readBytes
// Read a whole file. readText decodes as UTF-8. Returns an empty value if the
// file cannot be opened.
// ---------------------------------------------------------------------------
inline QByteArray readBytes(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
    return f.readAll();
}

inline QString readText(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(f.readAll());
}

// ---------------------------------------------------------------------------
// writeBytes / writeText / appendText
// Write a whole file, creating parent directories as needed. When append is
// false the write is atomic (via QSaveFile). Text is encoded as UTF-8.
// Returns true on success.
// ---------------------------------------------------------------------------
inline bool ensureDir(const QString &dirPath); // forward declaration

inline bool writeBytes(const QString &path, const QByteArray &data, bool append = false)
{
    const QString dir = QFileInfo(path).absolutePath();
    if (!dir.isEmpty())
        ensureDir(dir);

    if (append) {
        QFile f(path);
        if (!f.open(QIODevice::Append))
            return false;
        return f.write(data) == data.size();
    }

    // Atomic replace for the non-append case.
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    if (f.write(data) != data.size())
        return false;
    return f.commit();
}

inline bool writeText(const QString &path, const QString &text, bool append = false)
{
    return writeBytes(path, text.toUtf8(), append);
}

inline bool appendText(const QString &path, const QString &text)
{
    return writeText(path, text, true);
}

// ---------------------------------------------------------------------------
// exists / isFile / isDir / fileSize
// ---------------------------------------------------------------------------
inline bool exists(const QString &path)  { return QFileInfo::exists(path); }
inline bool isFile(const QString &path)  { return QFileInfo(path).isFile(); }
inline bool isDir(const QString &path)   { return QFileInfo(path).isDir();  }

// Size of a file in bytes, or -1 if it does not exist / is not a file.
inline qint64 fileSize(const QString &path)
{
    QFileInfo fi(path);
    return fi.isFile() ? fi.size() : -1;
}

// ---------------------------------------------------------------------------
// ensureDir — create dirPath (and any missing parents). Returns true if the
// directory exists afterwards.
// ---------------------------------------------------------------------------
inline bool ensureDir(const QString &dirPath)
{
    if (dirPath.isEmpty())
        return false;
    QDir dir;
    return dir.mkpath(dirPath);
}

// ---------------------------------------------------------------------------
// copyFile / moveFile / removeFile / removeDir
// ---------------------------------------------------------------------------
inline bool copyFile(const QString &src, const QString &dst, bool overwrite = false)
{
    const QString dir = QFileInfo(dst).absolutePath();
    if (!dir.isEmpty())
        ensureDir(dir);
    if (overwrite && QFile::exists(dst) && !QFile::remove(dst))
        return false;
    return QFile::copy(src, dst);
}

inline bool moveFile(const QString &src, const QString &dst, bool overwrite = false)
{
    const QString dir = QFileInfo(dst).absolutePath();
    if (!dir.isEmpty())
        ensureDir(dir);
    if (overwrite && QFile::exists(dst) && !QFile::remove(dst))
        return false;
    return QFile::rename(src, dst);
}

inline bool removeFile(const QString &path)
{
    if (!QFile::exists(path))
        return true;
    return QFile::remove(path);
}

// Recursively delete a directory and all of its contents.
inline bool removeDir(const QString &dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists())
        return true;
    return dir.removeRecursively();
}

// ---------------------------------------------------------------------------
// listFiles / listDirs
// List entries in dirPath. filters are wildcard patterns like {"*.png"}; an
// empty filter list matches everything. With recursive == true the whole tree
// is walked. Returned paths are absolute.
// ---------------------------------------------------------------------------
inline QStringList listFiles(const QString &dirPath,
                             const QStringList &filters = QStringList(),
                             bool recursive = false)
{
    QStringList result;
    QDirIterator it(dirPath, filters, QDir::Files | QDir::NoSymLinks,
                    recursive ? QDirIterator::Subdirectories
                              : QDirIterator::NoIteratorFlags);
    while (it.hasNext())
        result << it.next();
    result.sort();
    return result;
}

inline QStringList listDirs(const QString &dirPath, bool recursive = false)
{
    QStringList result;
    QDirIterator it(dirPath, QStringList(),
                    QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                    recursive ? QDirIterator::Subdirectories
                              : QDirIterator::NoIteratorFlags);
    while (it.hasNext())
        result << it.next();
    result.sort();
    return result;
}

// ---------------------------------------------------------------------------
// fileHashHex
// Compute a cryptographic hash of a file's contents and return it as a
// lower-case hex string. Reads the file in chunks so large files do not need
// to be loaded into memory. Returns an empty string on failure.
// ---------------------------------------------------------------------------
inline QString fileHashHex(const QString &path,
                           QCryptographicHash::Algorithm algo = QCryptographicHash::Sha256)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();

    QCryptographicHash hash(algo);
    if (!hash.addData(&f))
        return QString();
    return QString::fromLatin1(hash.result().toHex());
}

// ---------------------------------------------------------------------------
// Path helpers (thin wrappers over QFileInfo / QDir for convenience)
// ---------------------------------------------------------------------------
inline QString baseName(const QString &path)  { return QFileInfo(path).completeBaseName(); }
inline QString fileName(const QString &path)  { return QFileInfo(path).fileName(); }
inline QString suffix(const QString &path)    { return QFileInfo(path).suffix(); }
inline QString parentDir(const QString &path) { return QFileInfo(path).absolutePath(); }

inline QString joinPath(const QString &a, const QString &b)
{
    if (a.isEmpty()) return b;
    if (b.isEmpty()) return a;
    return QDir::cleanPath(a + '/' + b);
}

} // namespace QtUtil

#endif // QTUTIL_FILE_H
