#pragma once
#include <cstdio>

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QClipboard>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QStandardPaths>
#include <QString>
#include <QUrl>
#include <QtQmlIntegration/qqmlintegration.h>

// 轻量级线程安全文件日志器。
// - 静态方法可供任意线程（含 GraphExecutor 工作线程）直接调用。
// - 同时作为 QML 单例暴露，便于界面调用 path()/openFolder()。
class Log : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    explicit Log(QObject* parent = nullptr) : QObject(parent) {}
    ~Log() override { closeFile(); }

    static Log* instance() {
        static Log s;
        return &s;
    }

    // 打开日志文件（默认 AppDataLocation/ortdraw.log），超过 2MB 时轮转为 .1
    static void init(const QString& filePath = QString()) {
        instance()->initImpl(filePath);
    }
    static void info(const QString& msg)  { instance()->write(QStringLiteral("INFO"),  msg); }
    static void warn(const QString& msg)  { instance()->write(QStringLiteral("WARN"),  msg); }
    static void error(const QString& msg) { instance()->write(QStringLiteral("ERROR"), msg); }
    static void debug(const QString& msg) { instance()->write(QStringLiteral("DEBUG"), msg); }
    static QString filePath() { return instance()->m_path; }

    Q_INVOKABLE QString path() const { return m_path; }
    // 用系统默认程序打开日志文件本身
    Q_INVOKABLE void openFile() const {
        if (m_path.isEmpty()) return;
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_path));
    }
    // 打开日志所在文件夹
    Q_INVOKABLE void openFolder() const {
        if (m_path.isEmpty()) return;
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_path).absolutePath()));
    }
    // 复制日志文件路径到剪贴板
    Q_INVOKABLE void copyPath() const {
        if (m_path.isEmpty()) return;
        QGuiApplication::clipboard()->setText(m_path);
    }

private:
    static constexpr qint64 kMaxBytes = 2 * 1024 * 1024;

    void initImpl(const QString& filePath) {
        QMutexLocker lock(&m_mutex);
        if (m_file.isOpen()) m_file.close();
        QString path = filePath;
        if (path.isEmpty()) {
            path = QDir::tempPath() + QStringLiteral("/ortdraw.log");
        }
        QDir dir(QFileInfo(path).absolutePath());
        if (!dir.exists()) dir.mkpath(QStringLiteral("."));
        QFileInfo fi(path);
        if (fi.exists() && fi.size() > kMaxBytes) {
            const QString rotated = path + QStringLiteral(".1");
            QFile::remove(rotated);
            QFile::rename(path, rotated);
        }
        m_path = path;
        m_file.setFileName(path);
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            m_path.clear();
        qInstallMessageHandler(&Log::messageHandler);
    }

    // 追加一行并刷新，调用方无需持有锁
    void write(const QString& level, const QString& msg) {
        QMutexLocker lock(&m_mutex);
        writeLocked(level, msg);
    }

    // 需在持有 m_mutex 时调用
    void writeLocked(const QString& level, const QString& msg) {
        const QString line = QStringLiteral("[%1] [%2] %3\n")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
                 level, msg);
        if (m_file.isOpen()) {
            m_file.write(line.toUtf8());
            m_file.flush();
        }
    }

    // 将 qDebug/qWarning/qCritical 也写入日志文件，并保留 stderr 输出。
    // 处理器内不得再调用 qDebug 类函数，避免递归。
    static void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg) {
        static thread_local bool inHandler = false;
        if (inHandler) return;
        inHandler = true;
        QString level;
        switch (type) {
            case QtDebugMsg:    level = QStringLiteral("DEBUG"); break;
            case QtInfoMsg:     level = QStringLiteral("INFO");  break;
            case QtWarningMsg:  level = QStringLiteral("WARN");  break;
            case QtCriticalMsg: level = QStringLiteral("ERROR"); break;
            case QtFatalMsg:    level = QStringLiteral("FATAL"); break;
        }
        Log* self = instance();
        {
            QMutexLocker lock(&self->m_mutex);
            self->writeLocked(level, msg);
        }
        std::fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
        inHandler = false;
    }

    void closeFile() {
        QMutexLocker lock(&m_mutex);
        if (m_file.isOpen()) m_file.close();
    }

    QFile m_file;
    QMutex m_mutex;
    QString m_path;
};
