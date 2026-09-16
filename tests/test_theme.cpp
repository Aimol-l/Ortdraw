#include <QtTest>
#include <QSignalSpy>
#include "Theme.h"

class TestTheme : public QObject {
    Q_OBJECT
private slots:
    void defaultsToLight() {
        Theme t;
        QVERIFY(!t.dark());
        QCOMPARE(t.bg().name(), QString("#eef0f7"));
        QCOMPARE(t.bgPanel().name(), QString("#ffffff"));
        QCOMPARE(t.catInput().name(), QString("#007197"));
    }
    void toggleFlipsAndEmits() {
        Theme t;
        QSignalSpy spy(&t, &Theme::changed);
        t.toggle();
        QVERIFY(t.dark());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(t.bg().name(), QString("#1a1b26"));
        t.setDark(true);
        QCOMPARE(spy.count(), 1);
        t.setDark(false);
        QCOMPARE(spy.count(), 2);
    }
    void allTokensValid() {
        Theme t;
        for(bool d : {false, true}) {
            t.setDark(d);
            for(const QColor& c : {t.bg(), t.bgPanel(), t.bgElev(), t.bgHover(), t.border(),
                t.borderSoft(), t.fg(), t.fgDim(), t.fgBright(), t.blue(), t.cyan(), t.green(),
                t.yellow(), t.orange(), t.magenta(), t.red(), t.wire(), t.grid(), t.portIn(),
                t.portOut(), t.catInput(), t.catProcess(), t.catMath(), t.catOutput()}) {
                QVERIFY2(c.isValid(), "theme token must be a valid color");
            }
        }
        Theme a; a.setDark(false);
        Theme b; b.setDark(true);
        QVERIFY(a.bg() != b.bg());
        QVERIFY(a.fg() != b.fg());
        QVERIFY(a.catInput() != b.catInput());
        QVERIFY(a.portIn() == a.catInput());
        QVERIFY(a.portOut() == a.catOutput());
    }
};

QTEST_MAIN(TestTheme)
#include "test_theme.moc"
