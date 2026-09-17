#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <memory>
#include "Theme.h"
#include "Settings.h"

class TestTheme : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        m_tmp = std::make_unique<QTemporaryFile>();
        QVERIFY(m_tmp->open());
        qputenv("ORTDRAW_SETTINGS_PATH", m_tmp->fileName().toLocal8Bit());
    }
    void cleanupTestCase() {
        qunsetenv("ORTDRAW_SETTINGS_PATH");
    }
    void defaultsToLight() {
        Settings::settings()->resetDefaults();
        Theme t;
        QVERIFY(!t.dark());
        QCOMPARE(t.bg().name(), QString("#eef0f7"));
        QCOMPARE(t.bgPanel().name(), QString("#ffffff"));
        QCOMPARE(t.catInput().name(), QString("#007197"));
    }
    void toggleFlipsAndEmits() {
        Settings::settings()->resetDefaults();
        Theme t;
        QSignalSpy spy(&t, &Theme::changed);
        t.toggle();
        QVERIFY(t.dark());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(t.bg().name(), QString("#1b1f24"));
        t.setDark(true);
        QCOMPARE(spy.count(), 1);
        t.setDark(false);
        QCOMPARE(spy.count(), 2);
    }
    void allTokensValid() {
        Settings::settings()->resetDefaults();
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
        t.setDark(false);
        const QColor lightBg = t.bg(), lightFg = t.fg(), lightCat = t.catInput();
        const QColor lightBlue = t.blue();
        t.setDark(true);
        QVERIFY(t.bg() != lightBg);
        QVERIFY(t.fg() != lightFg);
        QVERIFY(t.catInput() != lightCat);
        QVERIFY(t.portIn() == t.catInput());
        QVERIFY(t.portOut() == t.catOutput());
        QCOMPARE(lightBlue.name(), QString("#2e7de9"));
        QCOMPARE(t.blue().name(), QString("#539bf5"));
        Settings::settings()->setAccentColor(QColor("#9854f1"));
        QCOMPARE(t.blue().name(), QString("#9854f1"));
        t.setDark(false);
        QCOMPARE(t.blue().name(), QString("#9854f1"));
    }
private:
    std::unique_ptr<QTemporaryFile> m_tmp;
};

QTEST_MAIN(TestTheme)
#include "test_theme.moc"
