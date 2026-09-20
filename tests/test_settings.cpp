#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <memory>
#include "Settings.h"
#include "Theme.h"

class TestSettings : public QObject {
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
    void defaultsAreCorrect() {
        QTemporaryFile f; QVERIFY(f.open());
        Settings s(f.fileName());
        QCOMPARE(s.theme(), QString("light"));
        QVERIFY(!s.accentCustom());
        QCOMPARE(s.backgroundMode(), QString("dots"));
        QCOMPARE(s.gridSpacing(), 26);
        QCOMPARE(s.renderMode(), QString("spline"));
        QCOMPARE(s.midpointMode(), QString("selected"));
        QCOMPARE(s.previewHeight(), 88);
        QCOMPARE(s.cornerRadius(), 10);
        QCOMPARE(s.minimapFps(), 30);
    }
    void writeEmitsAndClamps() {
        QTemporaryFile f; QVERIFY(f.open());
        Settings s(f.fileName());
        QSignalSpy spy(&s, &Settings::gridSpacingChanged);
        s.setGridSpacing(999);
        QCOMPARE(s.gridSpacing(), 60);       // clamped
        QCOMPARE(spy.count(), 1);
        s.setCornerRadius(-5);
        QCOMPARE(s.cornerRadius(), 0);
        s.setRenderMode("linear");
        QCOMPARE(s.renderMode(), QString("linear"));
        s.setRenderMode("bogus");
        QCOMPARE(s.renderMode(), QString("spline"));  // invalid -> default
    }
    void persistsAndReloads() {
        QString path;
        { QTemporaryFile f; QVERIFY(f.open()); path = f.fileName(); f.close(); }
        { Settings s(path); s.setTheme("dark"); s.setAccentColor(QColor("#9854f1")); s.sync(); }
        { Settings s2(path); QCOMPARE(s2.theme(), QString("dark"));
          QVERIFY(s2.accentCustom()); QCOMPARE(s2.accentColor().name(), QString("#9854f1")); }
    }
    void queueAnimationDefaultsAndPersists() {
        QTemporaryFile f; QVERIFY(f.open());
        Settings s(f.fileName());
        QVERIFY(s.queueAnimation());
        QSignalSpy spy(&s, &Settings::queueAnimationChanged);
        s.setQueueAnimation(false);
        QCOMPARE(spy.count(), 1);
        QVERIFY(!s.queueAnimation());
        Settings s2(f.fileName());
        QVERIFY(!s2.queueAnimation());
    }
    void resetDefaultsRestoresAndSignals() {
        QTemporaryFile f; QVERIFY(f.open());
        Settings s(f.fileName());
        s.setTheme("dark"); s.setBackgroundMode("none");
        QSignalSpy spy(&s, &Settings::themeChanged);
        s.resetDefaults();
        QCOMPARE(s.theme(), QString("light"));
        QCOMPARE(s.backgroundMode(), QString("dots"));
        QVERIFY(spy.count() >= 1);
    }
    void clearAccentCustomResetsFlagAndDefault() {
        // 局部实例：标志位、默认色恢复与持久化
        QTemporaryFile f; QVERIFY(f.open());
        Settings s(f.fileName());
        s.setAccentColor(QColor("#9854f1"));
        QVERIFY(s.accentCustom());
        QCOMPARE(s.accentColor().name(), QString("#9854f1"));
        QSignalSpy spy(&s, &Settings::accentColorChanged);
        s.clearAccentCustom();
        QVERIFY(!s.accentCustom());
        QCOMPARE(s.accentColor().name(), QString("#2e7de9"));
        QCOMPARE(spy.count(), 1);
        s.clearAccentCustom();
        QCOMPARE(spy.count(), 1);              // 幂等，不再发信号
        s.sync();
        Settings s2(f.fileName());
        QVERIFY(!s2.accentCustom());           // 重新加载后仍为非自定义
        QCOMPARE(s2.accentColor().name(), QString("#2e7de9"));

        // Theme 面向：非自定义时回到亮/暗默认强调色
        Settings* g = Settings::settings();
        g->resetDefaults();
        g->setAccentColor(QColor("#9854f1"));
        Theme t;
        QCOMPARE(t.blue().name(), QString("#9854f1"));
        g->clearAccentCustom();
        QCOMPARE(t.blue().name(), QString("#2e7de9"));   // 亮色默认
        t.setDark(true);
        QCOMPARE(t.blue().name(), QString("#539bf5"));   // 暗色默认
        t.setDark(false);
        g->resetDefaults();
    }
private:
    std::unique_ptr<QTemporaryFile> m_tmp;
};

QTEST_MAIN(TestSettings)
#include "test_settings.moc"
