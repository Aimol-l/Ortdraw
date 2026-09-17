#pragma once
#include <QObject>
#include <QColor>
#include <QtQmlIntegration/qqmlintegration.h>
#include "Settings.h"

class Theme : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY changed)
    Q_PROPERTY(QColor bg READ bg NOTIFY changed)
    Q_PROPERTY(QColor bgPanel READ bgPanel NOTIFY changed)
    Q_PROPERTY(QColor bgElev READ bgElev NOTIFY changed)
    Q_PROPERTY(QColor bgHover READ bgHover NOTIFY changed)
    Q_PROPERTY(QColor border READ border NOTIFY changed)
    Q_PROPERTY(QColor borderSoft READ borderSoft NOTIFY changed)
    Q_PROPERTY(QColor fg READ fg NOTIFY changed)
    Q_PROPERTY(QColor fgDim READ fgDim NOTIFY changed)
    Q_PROPERTY(QColor fgBright READ fgBright NOTIFY changed)
    Q_PROPERTY(QColor blue READ blue NOTIFY changed)
    Q_PROPERTY(QColor cyan READ cyan NOTIFY changed)
    Q_PROPERTY(QColor green READ green NOTIFY changed)
    Q_PROPERTY(QColor yellow READ yellow NOTIFY changed)
    Q_PROPERTY(QColor orange READ orange NOTIFY changed)
    Q_PROPERTY(QColor magenta READ magenta NOTIFY changed)
    Q_PROPERTY(QColor red READ red NOTIFY changed)
    Q_PROPERTY(QColor wire READ wire NOTIFY changed)
    Q_PROPERTY(QColor grid READ grid NOTIFY changed)
    Q_PROPERTY(QColor portIn READ portIn NOTIFY changed)
    Q_PROPERTY(QColor portOut READ portOut NOTIFY changed)
    Q_PROPERTY(QColor catInput READ catInput NOTIFY changed)
    Q_PROPERTY(QColor catProcess READ catProcess NOTIFY changed)
    Q_PROPERTY(QColor catMath READ catMath NOTIFY changed)
    Q_PROPERTY(QColor catOutput READ catOutput NOTIFY changed)
public:
    explicit Theme(QObject* parent = nullptr) : QObject(parent) {
        connect(Settings::settings(), &Settings::themeChanged, this, &Theme::changed);
        connect(Settings::settings(), &Settings::accentColorChanged, this, &Theme::changed);
    }
    static QObject* instance() { static Theme t; return &t; }
    static Theme* theme() { return static_cast<Theme*>(instance()); }

    bool dark() const { return Settings::settings()->theme() == "dark"; }
    void setDark(bool d) { Settings::settings()->setTheme(d ? "dark" : "light"); }
    Q_INVOKABLE void toggle() { setDark(!dark()); }

    QColor bg()        const { static const QColor l("#eef0f7"), d("#1b1f24"); return dark() ? d : l; }
    QColor bgPanel()   const { static const QColor l("#ffffff"), d("#22272e"); return dark() ? d : l; }
    QColor bgElev()    const { static const QColor l("#ffffff"), d("#2d333b"); return dark() ? d : l; }
    QColor bgHover()   const { static const QColor l("#e9ebf6"), d("#373e47"); return dark() ? d : l; }
    QColor border()    const { static const QColor l("#cfd3e6"), d("#373e47"); return dark() ? d : l; }
    QColor borderSoft()const { static const QColor l("#e4e6f2"), d("#2d333b"); return dark() ? d : l; }
    QColor fg()        const { static const QColor l("#4c5180"), d("#adbac7"); return dark() ? d : l; }
    QColor fgDim()     const { static const QColor l("#9095b8"), d("#768390"); return dark() ? d : l; }
    QColor fgBright()  const { static const QColor l("#1f2335"), d("#cdd9e5"); return dark() ? d : l; }
    QColor blue()      const {
        static const QColor l("#2e7de9"), d("#539bf5");
        Settings* s = Settings::settings();
        return s->accentCustom() ? s->accentColor() : (dark() ? d : l);
    }
    QColor cyan()      const { static const QColor l("#007197"), d("#39c5cf"); return dark() ? d : l; }
    QColor green()     const { static const QColor l("#587539"), d("#57ab5a"); return dark() ? d : l; }
    QColor yellow()    const { static const QColor l("#8c6c3e"), d("#c69026"); return dark() ? d : l; }
    QColor orange()    const { static const QColor l("#b15c00"), d("#e0823d"); return dark() ? d : l; }
    QColor magenta()   const { static const QColor l("#9854f1"), d("#b083f0"); return dark() ? d : l; }
    QColor red()       const { static const QColor l("#f52a65"), d("#e5534b"); return dark() ? d : l; }
    QColor wire()      const { static const QColor l("#b9bddb"), d("#4a545e"); return dark() ? d : l; }
    QColor grid()      const { static const QColor l("#d6d9e8"), d("#2d333b"); return dark() ? d : l; }
    QColor portIn()    const { return catInput(); }
    QColor portOut()   const { return catOutput(); }
    QColor catInput()  const { static const QColor l("#007197"), d("#39c5cf"); return dark() ? d : l; }
    QColor catProcess()const { static const QColor l("#2e7de9"), d("#539bf5"); return dark() ? d : l; }
    QColor catMath()   const { static const QColor l("#b15c00"), d("#e0823d"); return dark() ? d : l; }
    QColor catOutput() const { static const QColor l("#9854f1"), d("#b083f0"); return dark() ? d : l; }

signals:
    void changed();
};
