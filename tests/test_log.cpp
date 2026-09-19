#include <QtTest>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <thread>
#include <vector>

#include "Log.hpp"

class TestLog : public QObject {
    Q_OBJECT
private slots:
    void writesLevelsAndTimestamp() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("test.log");

        Log::init(path);
        Log::info(QStringLiteral("hello"));
        Log::warn(QStringLiteral("warn-msg"));
        Log::error(QStringLiteral("error-msg"));
        Log::debug(QStringLiteral("debug-msg"));

        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QString::fromUtf8(f.readAll());

        QVERIFY(content.contains(QStringLiteral("hello")));
        QVERIFY(content.contains(QStringLiteral("warn-msg")));
        QVERIFY(content.contains(QStringLiteral("error-msg")));
        QVERIFY(content.contains(QStringLiteral("debug-msg")));
        QVERIFY(content.contains(QStringLiteral("[INFO]")));
        QVERIFY(content.contains(QStringLiteral("[WARN]")));
        QVERIFY(content.contains(QStringLiteral("[ERROR]")));
        QVERIFY(content.contains(QStringLiteral("[DEBUG]")));

        const QRegularExpression ts(
            QStringLiteral("\\[\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}\\.\\d{3}\\]"));
        QVERIFY2(ts.match(content).hasMatch(), "log line must contain a timestamp");

        QCOMPARE(Log::filePath(), path);
        QCOMPARE(Log::instance()->path(), path);
    }

    void appendsToExistingFile() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("append.log");

        Log::init(path);
        Log::info(QStringLiteral("first"));
        Log::init(path);
        Log::info(QStringLiteral("second"));

        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QString::fromUtf8(f.readAll());
        QVERIFY(content.contains(QStringLiteral("first")));
        QVERIFY(content.contains(QStringLiteral("second")));
    }

    void threadSafeAppend() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("threads.log");
        Log::init(path);

        std::vector<std::thread> threads;
        for (int i = 0; i < 8; ++i)
            threads.emplace_back([i] {
                for (int n = 0; n < 50; ++n)
                    Log::info(QStringLiteral("thread-%1-%2").arg(i).arg(n));
            });
        for (auto& t : threads) t.join();

        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QString::fromUtf8(f.readAll());
        for (int i = 0; i < 8; ++i) {
            for (int n = 0; n < 50; ++n) {
                QVERIFY2(content.contains(QStringLiteral("thread-%1-%2").arg(i).arg(n)),
                         qPrintable(QStringLiteral("missing thread-%1-%2").arg(i).arg(n)));
            }
        }
    }
};

QTEST_MAIN(TestLog)
#include "test_log.moc"
