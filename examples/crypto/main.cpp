#include "../../src/crypto.h"

#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QString text = "hello 你好";

    // -----------------------------------------------------------------------
    // 1. Hash digests (hex)
    // -----------------------------------------------------------------------
    qInfo() << "[md5]   " << QtUtil::md5(text);
    qInfo() << "[sha1]  " << QtUtil::sha1(text);
    qInfo() << "[sha256]" << QtUtil::sha256(text);
    qInfo() << "[sha512]" << QtUtil::sha512(text);

    // -----------------------------------------------------------------------
    // 2. HMAC (keyed hash)
    // -----------------------------------------------------------------------
    qInfo() << "[hmacSha256]" << QtUtil::hmacSha256(text, "my-secret-key");

    // -----------------------------------------------------------------------
    // 3. Base64 — standard and URL-safe — round trip
    // -----------------------------------------------------------------------
    const QString b64 = QtUtil::base64Encode(text);
    qInfo() << "[base64]    " << b64 << "->" << QtUtil::base64DecodeText(b64);

    const QString url = QtUtil::base64UrlEncode(text);
    qInfo() << "[base64url] " << url << "->"
            << QString::fromUtf8(QtUtil::base64UrlDecode(url));

    // -----------------------------------------------------------------------
    // 4. Hex — round trip
    // -----------------------------------------------------------------------
    const QString hex = QtUtil::toHex(text.toUtf8());
    qInfo() << "[hex]       " << hex << "->"
            << QString::fromUtf8(QtUtil::fromHex(hex));

    // -----------------------------------------------------------------------
    // 5. XOR cipher — applying twice with the same key restores the data
    // -----------------------------------------------------------------------
    const QByteArray enc = QtUtil::xorCipher(text.toUtf8(), "key");
    const QByteArray dec = QtUtil::xorCipher(enc, "key");
    qInfo() << "[xorCipher] restored:" << (QString::fromUtf8(dec) == text);

    return 0;
}
