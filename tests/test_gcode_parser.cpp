#include <QCoreApplication>
#include <QFile>
#include <iostream>
#include "../src/grbl/GcodeParser.h"
#include "../src/grbl/TextToGcode/ShxFontParser.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "=== Test 1: Modal G-code behavior ===" << std::endl;
    QStringList gcode1;
    gcode1 << "G21 G90";
    gcode1 << "G0 X0 Y0 Z5";      // Rapid
    gcode1 << "G0 X10 Y10 Z5";    // Rapid (explicit G0)
    gcode1 << "G1 X20 Y20 Z0 F500"; // Linear
    gcode1 << "X30 Y30";           // Should be Linear (modal)
    gcode1 << "G0 X40 Y40 Z5";    // Rapid again
    gcode1 << "X50 Y50 Z5";       // Should be Rapid (modal)

    GcodeParser parser1;
    QList<GcodeMove> moves1 = parser1.parse(gcode1);

    std::cout << "Parsed " << moves1.size() << " moves:" << std::endl;
    for (const auto &m : moves1) {
        std::string type;
        switch (m.type) {
        case GcodeMove::Type::Rapid: type = "Rapid"; break;
        case GcodeMove::Type::Linear: type = "Linear"; break;
        case GcodeMove::Type::ArcCW: type = "ArcCW"; break;
        case GcodeMove::Type::ArcCCW: type = "ArcCCW"; break;
        }
        std::cout << "  Line " << m.lineNumber << " " << type
                  << " (" << m.start.x() << "," << m.start.y() << "," << m.start.z()
                  << ") -> (" << m.end.x() << "," << m.end.y() << "," << m.end.z() << ")"
                  << std::endl;
    }

    std::cout << std::endl << "=== Test 2: movesToGcode round-trip ===" << std::endl;
    GcodeMove rapid, linear;
    rapid.type = GcodeMove::Type::Rapid;
    rapid.start = QVector3D(0, 0, 5);
    rapid.end = QVector3D(10, 10, 5);

    linear.type = GcodeMove::Type::Linear;
    linear.start = QVector3D(10, 10, 5);
    linear.end = QVector3D(20, 20, 0);
    linear.feedRate = 500;

    QList<GcodeMove> testMoves;
    testMoves << rapid << linear;

    QStringList gcodeOut = GcodeParser::movesToGcode(testMoves);
    std::cout << "Generated G-code:" << std::endl;
    for (const auto &line : gcodeOut) {
        std::cout << "  " << line.toStdString() << std::endl;
    }

    std::cout << std::endl << "=== Test 3: Parse generated G-code ===" << std::endl;
    GcodeParser parser3;
    QList<GcodeMove> moves3 = parser3.parse(gcodeOut);
    std::cout << "Re-parsed " << moves3.size() << " moves:" << std::endl;
    for (const auto &m : moves3) {
        std::string type;
        switch (m.type) {
        case GcodeMove::Type::Rapid: type = "Rapid"; break;
        case GcodeMove::Type::Linear: type = "Linear"; break;
        }
        std::cout << "  " << type
                  << " (" << m.start.x() << "," << m.start.y() << "," << m.start.z()
                  << ") -> (" << m.end.x() << "," << m.end.y() << "," << m.end.z() << ")"
                  << std::endl;
    }

    // Check results
    bool test1Pass = moves1.size() == 6;
    if (moves1.size() == 6) {
        // Line 1,2 should be Rapid
        if (moves1[0].type != GcodeMove::Type::Rapid) test1Pass = false;
        if (moves1[1].type != GcodeMove::Type::Rapid) test1Pass = false;
        // Line 3,4 should be Linear
        if (moves1[2].type != GcodeMove::Type::Linear) test1Pass = false;
        if (moves1[3].type != GcodeMove::Type::Linear) test1Pass = false;
        // Line 5,6 should be Rapid
        if (moves1[4].type != GcodeMove::Type::Rapid) test1Pass = false;
        if (moves1[5].type != GcodeMove::Type::Rapid) test1Pass = false;
    }

    bool test3Pass = moves3.size() == 2;
    if (moves3.size() == 2) {
        if (moves3[0].type != GcodeMove::Type::Rapid) test3Pass = false;
        if (moves3[1].type != GcodeMove::Type::Linear) test3Pass = false;
    }

    std::cout << std::endl << "=== Results ===" << std::endl;
    std::cout << "Test 1 (Modal behavior): " << (test1Pass ? "PASS" : "FAIL") << std::endl;
    std::cout << "Test 3 (Round-trip): " << (test3Pass ? "PASS" : "FAIL") << std::endl;

    std::cout << std::endl << "=== Test 4: SHX bigfont Chinese glyph ===" << std::endl;
    bool test4Pass = true;
    QString shxPath = "C:/Program Files/浩辰软件/浩辰CAD2022/Fonts/chineset.shx";
    if (QFile::exists(shxPath)) {
        QFile shxFile(shxPath);
        if (!shxFile.open(QIODevice::ReadOnly)) {
            std::cout << "Cannot open SHX file: " << shxPath.toStdString() << std::endl;
            test4Pass = false;
        } else {
            ShxFontParser parser;
            auto glyphs = parser.parse(shxFile);
            int cjkCount = 0;
            for (auto it = glyphs.constBegin(); it != glyphs.constEnd(); ++it) {
                uint16_t u = it.key().unicode();
                if (u >= 0x4E00 && u <= 0x9FFF) {
                    ++cjkCount;
                }
            }
            bool hasCjkGlyphs = cjkCount > 100;
            std::cout << "Loaded glyph count: " << glyphs.size() << std::endl;
            std::cout << "Detected CJK glyphs: " << cjkCount << std::endl;
            test4Pass = hasCjkGlyphs;
        }
    } else {
        std::cout << "SKIP: SHX file not found at " << shxPath.toStdString() << std::endl;
    }

    std::cout << "Test 4 (Chinese glyph parse): " << (test4Pass ? "PASS" : "FAIL") << std::endl;

    return (test1Pass && test3Pass && test4Pass) ? 0 : 1;
}
