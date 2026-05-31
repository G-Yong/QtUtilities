#include "../../src/file.h"

#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QString dir = "out";

    // -----------------------------------------------------------------------
    // 1. Text I/O (UTF-8) — write, append, read back
    // -----------------------------------------------------------------------
    QtUtil::writeText(dir + "/note.txt", "你好 world\n");
    QtUtil::appendText(dir + "/note.txt", "second line\n");
    qInfo() << "[readText]\n" << QtUtil::readText(dir + "/note.txt");

    // -----------------------------------------------------------------------
    // 2. Binary I/O
    // -----------------------------------------------------------------------
    QtUtil::writeBytes(dir + "/data.bin", QByteArray("\x01\x02\x03\x04", 4));
    qInfo() << "[fileSize] data.bin:" << QtUtil::fileSize(dir + "/data.bin") << "bytes";

    // -----------------------------------------------------------------------
    // 3. Filesystem operations
    // -----------------------------------------------------------------------
    QtUtil::ensureDir(dir + "/sub");
    QtUtil::copyFile(dir + "/note.txt", dir + "/sub/note_copy.txt", true);
    QtUtil::moveFile(dir + "/data.bin", dir + "/sub/data.bin", true);
    qInfo() << "[exists] sub/note_copy.txt:" << QtUtil::exists(dir + "/sub/note_copy.txt");
    qInfo() << "[exists] data.bin moved   :" << !QtUtil::exists(dir + "/data.bin");

    // -----------------------------------------------------------------------
    // 4. Listing
    // -----------------------------------------------------------------------
    qInfo() << "[listFiles] *.txt recursive:"
            << QtUtil::listFiles(dir, {"*.txt"}, true);
    qInfo() << "[listDirs]:" << QtUtil::listDirs(dir);

    // -----------------------------------------------------------------------
    // 5. Hashing & path helpers
    // -----------------------------------------------------------------------
    qInfo() << "[fileHashHex] note.txt SHA-256:"
            << QtUtil::fileHashHex(dir + "/note.txt");
    qInfo() << "[baseName]:" << QtUtil::baseName("a/b/report.final.txt");
    qInfo() << "[suffix]  :" << QtUtil::suffix("a/b/report.final.txt");
    qInfo() << "[joinPath]:" << QtUtil::joinPath("a/b/", "/c/d.txt");

    // Clean up the demo directory.
    QtUtil::removeDir(dir);
    return 0;
}
