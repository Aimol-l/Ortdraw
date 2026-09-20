#pragma once
#include <QObject>
#include <QColor>
#include <QSettings>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>
#include <memory>
#include "Log.hpp"

class Settings : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY accentColorChanged)
    Q_PROPERTY(bool accentCustom READ accentCustom NOTIFY accentColorChanged)
    Q_PROPERTY(QString density READ density WRITE setDensity NOTIFY densityChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString backgroundMode READ backgroundMode WRITE setBackgroundMode NOTIFY backgroundModeChanged)
    Q_PROPERTY(bool snapToGrid READ snapToGrid WRITE setSnapToGrid NOTIFY snapToGridChanged)
    Q_PROPERTY(int gridSpacing READ gridSpacing WRITE setGridSpacing NOTIFY gridSpacingChanged)
    Q_PROPERTY(bool spaceToPan READ spaceToPan WRITE setSpaceToPan NOTIFY spaceToPanChanged)
    Q_PROPERTY(int fitMargin READ fitMargin WRITE setFitMargin NOTIFY fitMarginChanged)
    Q_PROPERTY(qreal zoomMin READ zoomMin WRITE setZoomMin NOTIFY zoomRangeChanged)
    Q_PROPERTY(qreal zoomMax READ zoomMax WRITE setZoomMax NOTIFY zoomRangeChanged)
    Q_PROPERTY(bool showPreview READ showPreview WRITE setShowPreview NOTIFY showPreviewChanged)
    Q_PROPERTY(int previewHeight READ previewHeight WRITE setPreviewHeight NOTIFY previewHeightChanged)
    Q_PROPERTY(bool previewFullRes READ previewFullRes WRITE setPreviewFullRes NOTIFY previewFullResChanged)
    Q_PROPERTY(bool showPortTypeTags READ showPortTypeTags WRITE setShowPortTypeTags NOTIFY showPortTypeTagsChanged)
    Q_PROPERTY(bool autoHeight READ autoHeight WRITE setAutoHeight NOTIFY autoHeightChanged)
    Q_PROPERTY(QString textRender READ textRender WRITE setTextRender NOTIFY textRenderChanged)
    Q_PROPERTY(int cornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY cornerRadiusChanged)
    Q_PROPERTY(QString renderMode READ renderMode WRITE setRenderMode NOTIFY renderModeChanged)
    Q_PROPERTY(int linkWidth READ linkWidth WRITE setLinkWidth NOTIFY linkWidthChanged)
    Q_PROPERTY(QString midpointMode READ midpointMode WRITE setMidpointMode NOTIFY midpointModeChanged)
    Q_PROPERTY(bool hoverHighlight READ hoverHighlight WRITE setHoverHighlight NOTIFY hoverHighlightChanged)
    Q_PROPERTY(bool ctrlMultiSelect READ ctrlMultiSelect WRITE setCtrlMultiSelect NOTIFY ctrlMultiSelectChanged)
    Q_PROPERTY(bool contextMenu READ contextMenu WRITE setContextMenu NOTIFY contextMenuChanged)
    Q_PROPERTY(bool confirmDelete READ confirmDelete WRITE setConfirmDelete NOTIFY confirmDeleteChanged)
    Q_PROPERTY(QString connectMode READ connectMode WRITE setConnectMode NOTIFY connectModeChanged)
    Q_PROPERTY(bool autoDisconnect READ autoDisconnect WRITE setAutoDisconnect NOTIFY autoDisconnectChanged)
    Q_PROPERTY(int minimapFps READ minimapFps WRITE setMinimapFps NOTIFY minimapFpsChanged)
    Q_PROPERTY(bool antialias READ antialias WRITE setAntialias NOTIFY antialiasChanged)
    Q_PROPERTY(bool asyncImage READ asyncImage WRITE setAsyncImage NOTIFY asyncImageChanged)
public:
    explicit Settings(QObject* parent = nullptr)
        : QObject(parent) {
        const QString overridePath = QString::fromLocal8Bit(qgetenv("ORTDRAW_SETTINGS_PATH"));
        if (!overridePath.isEmpty())
            m_store = std::make_unique<QSettings>(overridePath, QSettings::IniFormat);
        else
            m_store = std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope,
                                                  QStringLiteral("Ortdraw"), QStringLiteral("Ortdraw"));
        cleanupLegacyKeys();
        load();
    }

    explicit Settings(const QString& iniPath, QObject* parent = nullptr)
        : QObject(parent)
        , m_store(std::make_unique<QSettings>(iniPath, QSettings::IniFormat)) {
        load();
        cleanupLegacyKeys();
    }
    // 清理已废弃的历史键
    void cleanupLegacyKeys() {
        m_store->remove("canvas/showGrid");   // 已由 canvas/background 取代
    }


    ~Settings() override { m_store->sync(); }

    static QObject* instance() {
        static Settings s;
        return &s;
    }
    static Settings* settings() { return static_cast<Settings*>(instance()); }

    QString theme() const { return m_theme; }
    QColor accentColor() const { return m_accentColor; }
    bool accentCustom() const { return m_accentCustom; }
    QString density() const { return m_density; }
    QString language() const { return m_language; }
    QString backgroundMode() const { return m_backgroundMode; }
    bool snapToGrid() const { return m_snapToGrid; }
    int gridSpacing() const { return m_gridSpacing; }
    bool spaceToPan() const { return m_spaceToPan; }
    int fitMargin() const { return m_fitMargin; }
    qreal zoomMin() const { return m_zoomMin; }
    qreal zoomMax() const { return m_zoomMax; }
    bool showPreview() const { return m_showPreview; }
    int previewHeight() const { return m_previewHeight; }
    bool previewFullRes() const { return m_previewFullRes; }
    bool showPortTypeTags() const { return m_showPortTypeTags; }
    bool autoHeight() const { return m_autoHeight; }
    QString textRender() const { return m_textRender; }
    int cornerRadius() const { return m_cornerRadius; }
    QString renderMode() const { return m_renderMode; }
    int linkWidth() const { return m_linkWidth; }
    QString midpointMode() const { return m_midpointMode; }
    bool hoverHighlight() const { return m_hoverHighlight; }
    bool ctrlMultiSelect() const { return m_ctrlMultiSelect; }
    bool contextMenu() const { return m_contextMenu; }
    bool confirmDelete() const { return m_confirmDelete; }
    QString connectMode() const { return m_connectMode; }
    bool autoDisconnect() const { return m_autoDisconnect; }
    int minimapFps() const { return m_minimapFps; }
    bool antialias() const { return m_antialias; }
    bool asyncImage() const { return m_asyncImage; }

    void setTheme(const QString& v) {
        const QString n = oneOf(v, {"light", "dark"}, "light");
        if (m_theme == n) return;
        m_theme = n; m_store->setValue("appearance/theme", n); emit themeChanged();
    }
    void setAccentColor(const QColor& c) {
        if (!c.isValid()) return;
        m_accentColor = c; m_accentCustom = true;
        m_store->setValue("appearance/accentColor", c.name());
        m_store->setValue("appearance/accentCustom", true);
        emit accentColorChanged();
    }
    void setDensity(const QString& v) {
        if (m_density == v) return;
        m_density = v; m_store->setValue("appearance/density", v); emit densityChanged();
    }
    void setLanguage(const QString& v) {
        if (m_language == v) return;
        m_language = v; m_store->setValue("appearance/language", v); emit languageChanged();
    }
    void setBackgroundMode(const QString& v) {
        const QString n = (v == "none" || v == "dots" || v == "grid") ? v : "dots";
        if (m_backgroundMode == n) return;
        m_backgroundMode = n; m_store->setValue("canvas/background", n); emit backgroundModeChanged();
    }
    void setSnapToGrid(bool v) {
        if (m_snapToGrid == v) return;
        m_snapToGrid = v; m_store->setValue("canvas/snapToGrid", v); emit snapToGridChanged();
    }
    void setGridSpacing(int v) {
        v = qBound(10, v, 60);
        if (m_gridSpacing == v) return;
        m_gridSpacing = v; m_store->setValue("canvas/gridSpacing", v); emit gridSpacingChanged();
    }
    void setSpaceToPan(bool v) {
        if (m_spaceToPan == v) return;
        m_spaceToPan = v; m_store->setValue("canvas/spaceToPan", v); emit spaceToPanChanged();
    }
    void setFitMargin(int v) {
        v = qBound(20, v, 200);
        if (m_fitMargin == v) return;
        m_fitMargin = v; m_store->setValue("canvas/fitMargin", v); emit fitMarginChanged();
    }
    void setZoomMin(qreal v) {
        v = qBound(0.1, v, 1.0);
        if (qFuzzyCompare(m_zoomMin, v)) return;
        m_zoomMin = v;
        if (m_zoomMax <= m_zoomMin) m_zoomMax = qMin(m_zoomMin + 0.1, 8.0);
        m_store->setValue("canvas/zoomMin", m_zoomMin);
        m_store->setValue("canvas/zoomMax", m_zoomMax);
        emit zoomRangeChanged();
    }
    void setZoomMax(qreal v) {
        v = qBound(1.0, v, 8.0);
        if (qFuzzyCompare(m_zoomMax, v)) return;
        m_zoomMax = v;
        if (m_zoomMin >= m_zoomMax) m_zoomMin = qMax(m_zoomMax - 0.1, 0.1);
        m_store->setValue("canvas/zoomMin", m_zoomMin);
        m_store->setValue("canvas/zoomMax", m_zoomMax);
        emit zoomRangeChanged();
    }
    void setShowPreview(bool v) {
        if (m_showPreview == v) return;
        m_showPreview = v; m_store->setValue("nodes/showPreview", v); emit showPreviewChanged();
    }
    void setPreviewHeight(int v) {
        v = qBound(60, v, 160);
        if (m_previewHeight == v) return;
        m_previewHeight = v; m_store->setValue("nodes/previewHeight", v); emit previewHeightChanged();
    }
    void setPreviewFullRes(bool v) {
        if (m_previewFullRes == v) return;
        m_previewFullRes = v; m_store->setValue("nodes/previewFullRes", v); emit previewFullResChanged();
    }
    void setShowPortTypeTags(bool v) {
        if (m_showPortTypeTags == v) return;
        m_showPortTypeTags = v; m_store->setValue("nodes/showPortTypeTags", v); emit showPortTypeTagsChanged();
    }
    void setAutoHeight(bool v) {
        if (m_autoHeight == v) return;
        m_autoHeight = v; m_store->setValue("nodes/autoHeight", v); emit autoHeightChanged();
    }
    void setTextRender(const QString& v) {
        const QString n = oneOf(v, {"curve", "native"}, "curve");
        if (m_textRender == n) return;
        m_textRender = n; m_store->setValue("nodes/textRender", n); emit textRenderChanged();
    }
    void setCornerRadius(int v) {
        v = qBound(0, v, 20);
        if (m_cornerRadius == v) return;
        m_cornerRadius = v; m_store->setValue("nodes/cornerRadius", v); emit cornerRadiusChanged();
    }
    void setRenderMode(const QString& v) {
        const QString n = oneOf(v, {"spline", "linear", "straight"}, "spline");
        if (m_renderMode == n) return;
        m_renderMode = n; m_store->setValue("links/renderMode", n); emit renderModeChanged();
    }
    void setLinkWidth(int v) {
        v = qBound(1, v, 5);
        if (m_linkWidth == v) return;
        m_linkWidth = v; m_store->setValue("links/width", v); emit linkWidthChanged();
    }
    void setMidpointMode(const QString& v) {
        const QString n = oneOf(v, {"selected", "hover", "always", "never"}, "selected");
        if (m_midpointMode == n) return;
        m_midpointMode = n; m_store->setValue("links/midpointMode", n); emit midpointModeChanged();
    }
    void setHoverHighlight(bool v) {
        if (m_hoverHighlight == v) return;
        m_hoverHighlight = v; m_store->setValue("links/hoverHighlight", v); emit hoverHighlightChanged();
    }
    void setCtrlMultiSelect(bool v) {
        if (m_ctrlMultiSelect == v) return;
        m_ctrlMultiSelect = v; m_store->setValue("interaction/ctrlMultiSelect", v); emit ctrlMultiSelectChanged();
    }
    void setContextMenu(bool v) {
        if (m_contextMenu == v) return;
        m_contextMenu = v; m_store->setValue("interaction/contextMenu", v); emit contextMenuChanged();
    }
    void setConfirmDelete(bool v) {
        if (m_confirmDelete == v) return;
        m_confirmDelete = v; m_store->setValue("interaction/confirmDelete", v); emit confirmDeleteChanged();
    }
    void setConnectMode(const QString& v) {
        const QString n = oneOf(v, {"click", "drag"}, "drag");
        if (m_connectMode == n) return;
        m_connectMode = n; m_store->setValue("interaction/connectMode", n); emit connectModeChanged();
    }
    void setAutoDisconnect(bool v) {
        if (m_autoDisconnect == v) return;
        m_autoDisconnect = v; m_store->setValue("interaction/autoDisconnect", v); emit autoDisconnectChanged();
    }
    void setMinimapFps(int v) {
        if (v != 0 && v != 30 && v != 60) v = 30;
        if (m_minimapFps == v) return;
        m_minimapFps = v; m_store->setValue("perf/minimapFps", v); emit minimapFpsChanged();
    }
    void setAntialias(bool v) {
        if (m_antialias == v) return;
        m_antialias = v; m_store->setValue("perf/antialias", v); emit antialiasChanged();
    }
    void setAsyncImage(bool v) {
        if (m_asyncImage == v) return;
        m_asyncImage = v; m_store->setValue("perf/asyncImage", v); emit asyncImageChanged();
    }

    Q_INVOKABLE void resetDefaults() {
        Log::info(QStringLiteral("恢复默认设置"));
        m_theme = "light"; m_store->setValue("appearance/theme", m_theme);
        m_accentColor = QColor("#2e7de9"); m_accentCustom = false;
        m_store->setValue("appearance/accentColor", m_accentColor.name());
        m_store->setValue("appearance/accentCustom", false);
        m_density = "standard"; m_store->setValue("appearance/density", m_density);
        m_language = "zh_CN"; m_store->setValue("appearance/language", m_language);
        m_backgroundMode = "dots"; m_store->setValue("canvas/background", m_backgroundMode);
        m_snapToGrid = false; m_store->setValue("canvas/snapToGrid", m_snapToGrid);
        m_gridSpacing = 26; m_store->setValue("canvas/gridSpacing", m_gridSpacing);
        m_spaceToPan = true; m_store->setValue("canvas/spaceToPan", m_spaceToPan);
        m_fitMargin = 80; m_store->setValue("canvas/fitMargin", m_fitMargin);
        m_zoomMin = 0.35; m_store->setValue("canvas/zoomMin", m_zoomMin);
        m_zoomMax = 2.4; m_store->setValue("canvas/zoomMax", m_zoomMax);
        m_showPreview = true; m_store->setValue("nodes/showPreview", m_showPreview);
        m_previewHeight = 88; m_store->setValue("nodes/previewHeight", m_previewHeight);
        m_previewFullRes = false; m_store->setValue("nodes/previewFullRes", m_previewFullRes);
        m_showPortTypeTags = true; m_store->setValue("nodes/showPortTypeTags", m_showPortTypeTags);
        m_autoHeight = true; m_store->setValue("nodes/autoHeight", m_autoHeight);
        m_textRender = "curve"; m_store->setValue("nodes/textRender", m_textRender);
        m_cornerRadius = 10; m_store->setValue("nodes/cornerRadius", m_cornerRadius);
        m_renderMode = "spline"; m_store->setValue("links/renderMode", m_renderMode);
        m_linkWidth = 2; m_store->setValue("links/width", m_linkWidth);
        m_midpointMode = "selected"; m_store->setValue("links/midpointMode", m_midpointMode);
        m_hoverHighlight = true; m_store->setValue("links/hoverHighlight", m_hoverHighlight);
        m_ctrlMultiSelect = true; m_store->setValue("interaction/ctrlMultiSelect", m_ctrlMultiSelect);
        m_contextMenu = true; m_store->setValue("interaction/contextMenu", m_contextMenu);
        m_confirmDelete = false; m_store->setValue("interaction/confirmDelete", m_confirmDelete);
        m_connectMode = "drag"; m_store->setValue("interaction/connectMode", m_connectMode);
        m_autoDisconnect = false; m_store->setValue("interaction/autoDisconnect", m_autoDisconnect);
        m_minimapFps = 30; m_store->setValue("perf/minimapFps", m_minimapFps);
        m_antialias = true; m_store->setValue("perf/antialias", m_antialias);
        m_asyncImage = true; m_store->setValue("perf/asyncImage", m_asyncImage);

        emit themeChanged();
        emit accentColorChanged();
        emit densityChanged();
        emit languageChanged();
        emit backgroundModeChanged();
        emit snapToGridChanged();
        emit gridSpacingChanged();
        emit spaceToPanChanged();
        emit fitMarginChanged();
        emit zoomRangeChanged();
        emit showPreviewChanged();
        emit previewHeightChanged();
        emit previewFullResChanged();
        emit showPortTypeTagsChanged();
        emit autoHeightChanged();
        emit textRenderChanged();
        emit cornerRadiusChanged();
        emit renderModeChanged();
        emit linkWidthChanged();
        emit midpointModeChanged();
        emit hoverHighlightChanged();
        emit ctrlMultiSelectChanged();
        emit contextMenuChanged();
        emit confirmDeleteChanged();
        emit connectModeChanged();
        emit autoDisconnectChanged();
        emit minimapFpsChanged();
        emit antialiasChanged();
        emit asyncImageChanged();
    }

    Q_INVOKABLE void sync() { m_store->sync(); }

    // 取消自定义强调色：恢复默认色并清除自定义标记（供设置对话框取消时回滚）
    Q_INVOKABLE void clearAccentCustom() {
        if (!m_accentCustom) return;
        m_accentCustom = false;
        m_accentColor = QColor("#2e7de9");
        m_store->setValue("appearance/accentColor", m_accentColor.name());
        m_store->remove("appearance/accentCustom");
        emit accentColorChanged();
    }

signals:
    void themeChanged();
    void accentColorChanged();
    void densityChanged();
    void languageChanged();
    void backgroundModeChanged();
    void snapToGridChanged();
    void gridSpacingChanged();
    void spaceToPanChanged();
    void fitMarginChanged();
    void zoomRangeChanged();
    void showPreviewChanged();
    void previewHeightChanged();
    void previewFullResChanged();
    void showPortTypeTagsChanged();
    void autoHeightChanged();
    void textRenderChanged();
    void cornerRadiusChanged();
    void renderModeChanged();
    void linkWidthChanged();
    void midpointModeChanged();
    void hoverHighlightChanged();
    void ctrlMultiSelectChanged();
    void contextMenuChanged();
    void confirmDeleteChanged();
    void connectModeChanged();
    void autoDisconnectChanged();
    void minimapFpsChanged();
    void antialiasChanged();
    void asyncImageChanged();

private:
    static QString oneOf(const QString& v, const QStringList& allowed, const QString& def) {
        return allowed.contains(v) ? v : def;
    }

    void load() {
        m_theme = oneOf(m_store->value("appearance/theme", "light").toString(),
                        {"light", "dark"}, "light");
        m_accentColor = QColor(m_store->value("appearance/accentColor", "#2e7de9").toString());
        if (!m_accentColor.isValid()) m_accentColor = QColor("#2e7de9");
        m_accentCustom = m_store->value("appearance/accentCustom", false).toBool();
        m_density = m_store->value("appearance/density", "standard").toString();
        m_language = m_store->value("appearance/language", "zh_CN").toString();
        { const QString bm = m_store->value("canvas/background", "dots").toString();
          m_backgroundMode = (bm == "none" || bm == "dots" || bm == "grid") ? bm : "dots"; }
        m_snapToGrid = m_store->value("canvas/snapToGrid", false).toBool();
        m_gridSpacing = qBound(10, m_store->value("canvas/gridSpacing", 26).toInt(), 60);
        m_spaceToPan = m_store->value("canvas/spaceToPan", true).toBool();
        m_fitMargin = qBound(20, m_store->value("canvas/fitMargin", 80).toInt(), 200);
        m_zoomMin = qBound(0.1, m_store->value("canvas/zoomMin", 0.35).toDouble(), 1.0);
        m_zoomMax = qBound(1.0, m_store->value("canvas/zoomMax", 2.4).toDouble(), 8.0);
        if (m_zoomMin >= m_zoomMax) m_zoomMax = qMin(m_zoomMin + 0.1, 8.0);
        m_showPreview = m_store->value("nodes/showPreview", true).toBool();
        m_previewHeight = qBound(60, m_store->value("nodes/previewHeight", 88).toInt(), 160);
        m_previewFullRes = m_store->value("nodes/previewFullRes", false).toBool();
        m_showPortTypeTags = m_store->value("nodes/showPortTypeTags", true).toBool();
        m_autoHeight = m_store->value("nodes/autoHeight", true).toBool();
        m_textRender = oneOf(m_store->value("nodes/textRender", "curve").toString(),
                             {"curve", "native"}, "curve");
        m_cornerRadius = qBound(0, m_store->value("nodes/cornerRadius", 10).toInt(), 20);
        m_renderMode = oneOf(m_store->value("links/renderMode", "spline").toString(),
                             {"spline", "linear", "straight"}, "spline");
        m_linkWidth = qBound(1, m_store->value("links/width", 2).toInt(), 5);
        m_midpointMode = oneOf(m_store->value("links/midpointMode", "selected").toString(),
                               {"selected", "hover", "always", "never"}, "selected");
        m_hoverHighlight = m_store->value("links/hoverHighlight", true).toBool();
        m_ctrlMultiSelect = m_store->value("interaction/ctrlMultiSelect", true).toBool();
        m_contextMenu = m_store->value("interaction/contextMenu", true).toBool();
        m_confirmDelete = m_store->value("interaction/confirmDelete", false).toBool();
        m_connectMode = oneOf(m_store->value("interaction/connectMode", "drag").toString(),
                              {"click", "drag"}, "drag");
        m_autoDisconnect = m_store->value("interaction/autoDisconnect", false).toBool();
        m_minimapFps = m_store->value("perf/minimapFps", 30).toInt();
        if (m_minimapFps != 0 && m_minimapFps != 30 && m_minimapFps != 60) m_minimapFps = 30;
        m_antialias = m_store->value("perf/antialias", true).toBool();
        m_asyncImage = m_store->value("perf/asyncImage", true).toBool();
    }

    std::unique_ptr<QSettings> m_store;

    QString m_theme = "light";
    QColor m_accentColor = QColor("#2e7de9");
    bool m_accentCustom = false;
    QString m_density = "standard";
    QString m_language = "zh_CN";
    QString m_backgroundMode = "dots";
    bool m_snapToGrid = false;
    int m_gridSpacing = 26;
    bool m_spaceToPan = true;
    int m_fitMargin = 80;
    qreal m_zoomMin = 0.35;
    qreal m_zoomMax = 2.4;
    bool m_showPreview = true;
    int m_previewHeight = 88;
    bool m_previewFullRes = false;
    bool m_showPortTypeTags = true;
    bool m_autoHeight = true;
    QString m_textRender = "curve";
    int m_cornerRadius = 10;
    QString m_renderMode = "spline";
    int m_linkWidth = 2;
    QString m_midpointMode = "selected";
    bool m_hoverHighlight = true;
    bool m_ctrlMultiSelect = true;
    bool m_contextMenu = true;
    bool m_confirmDelete = false;
    QString m_connectMode = "drag";
    bool m_autoDisconnect = false;
    int m_minimapFps = 30;
    bool m_antialias = true;
    bool m_asyncImage = true;
};
