#include <QCoreApplication>
#include <QFile>
#include <QByteArray>
#include <QDebug>
#include <iostream>
#include <QTextCodec>
#include "../src/grbl/TextToGcode/ShxFontParser.h"

void testFont(const QString& fontPath, const QString& label) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Testing: " << label.toStdString() << std::endl;
    std::cout << "========================================" << std::endl;

    QFile f(fontPath);
    if (!f.open(QIODevice::ReadOnly)) {
        std::cerr << "Cannot open: " << fontPath.toStdString() << std::endl;
        return;
    }

    ShxFontParser parser;
    parser.setFilePath(fontPath);
    auto glyphMap = parser.parse(f);
    f.close();

    // Test chars: yi(一) U+4E00, zhong(中) U+4E2D, ren(人) U+4EBA,
    //             ni(你) U+4F60, hao(好) U+597D
    struct TestChar { uint code; const char* name; };
    TestChar tests[] = {
        {0x4E00, "yi(一)"},
        {0x4E2D, "zhong(中)"},
        {0x4EBA, "ren(人)"},
        {0x4F60, "ni(你)"},
        {0x597D, "hao(好)"},
    };

    int cjkTotal = 0;
    for (auto it = glyphMap.constBegin(); it != glyphMap.constEnd(); ++it) {
        if (it.key().unicode() > 0xFF) cjkTotal++;
    }
    std::cout << "Total CJK glyphs loaded: " << cjkTotal << std::endl;

    for (const auto& tc : tests) {
        QChar ch(tc.code);
        if (glyphMap.contains(ch)) {
            const auto& strokes = glyphMap[ch];
            std::cout << "\n  " << tc.name << " U+" << QString::number(tc.code, 16).toStdString()
                      << ": " << strokes.size() << " strokes" << std::endl;
            for (int i = 0; i < strokes.size(); ++i) {
                const auto& pts = strokes[i];
                std::cout << "    Stroke " << i << " (" << pts.size() << " pts): ("
                          << pts.first().x() << "," << pts.first().y() << ") -> ("
                          << pts.last().x() << "," << pts.last().y() << ")" << std::endl;
            }
        } else {
            std::cout << "\n  " << tc.name << " U+" << QString::number(tc.code, 16).toStdString()
                      << ": NOT FOUND" << std::endl;
        }
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    // Test codec availability
    QTextCodec* gbkCodec = QTextCodec::codecForName("GBK");
    std::cout << "GBK codec: " << (gbkCodec ? "available" : "NOT available") << std::endl;

    testFont("D:/projects/github/grbl_cpp/bigfonts/HZDX.SHX", "HZDX.SHX");
    testFont("D:/projects/github/grbl_cpp/bigfonts/gbcbig.shx", "gbcbig.shx");
    testFont("D:/projects/github/grbl_cpp/bigfonts/bigfont.shx", "bigfont.shx");

    return 0;
}
