#include "ThemeManager.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

namespace{
    const char* kTemplatePath = ":/style/stylesheet.tokened.qss";
    const char* kIniGroup     = "theme";
}

// ---------------------------------------------------------------- 持久化
    void ThemeManager::load(){
        const QString dir =
            QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(dir);                       // 首次不存在，必须建

        QSettings s(dir + QStringLiteral("/theme.ini"), QSettings::IniFormat);
        s.beginGroup(QString::fromUtf8(kIniGroup));

        ThemeTokens t = defaultTokens();          // 读不到的键保持默认
        t.sideBar          = s.value("sideBar",          t.sideBar).toString();
        t.bg               = s.value("bg",               t.bg).toString();
        t.surface          = s.value("surface",          t.surface).toString();
        t.surfaceAlt       = s.value("surfaceAlt",       t.surfaceAlt).toString();
        t.hover            = s.value("hover",            t.hover).toString();
        t.chipBg           = s.value("chipBg",           t.chipBg).toString();
        t.text             = s.value("text",             t.text).toString();
        t.textStrong       = s.value("textStrong",       t.textStrong).toString();
        t.textMuted        = s.value("textMuted",        t.textMuted).toString();
        t.accentText       = s.value("accentText",       t.accentText).toString();
        t.accentTextAlt    = s.value("accentTextAlt",    t.accentTextAlt).toString();
        t.onAccent         = s.value("onAccent",         t.onAccent).toString();
        t.border           = s.value("border",           t.border).toString();
        t.scrollbar        = s.value("scrollbar",        t.scrollbar).toString();
        t.accentFill       = s.value("accentFill",       t.accentFill).toString();
        t.accentFillHover  = s.value("accentFillHover",  t.accentFillHover).toString();
        t.danger           = s.value("danger",           t.danger).toString();
        t.success          = s.value("success",          t.success).toString();

        s.endGroup();

        setTokens(t);                             // _dirty = true
    }

    void ThemeManager::save() const{
        const QString dir =
            QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(dir);

        QSettings s(dir + QStringLiteral("/theme.ini"), QSettings::IniFormat);  //这里并不会if文件不存在就立即生成
        s.beginGroup(QString::fromUtf8(kIniGroup));

        s.setValue("sideBar",         _tokens.sideBar);
        s.setValue("bg",              _tokens.bg);
        s.setValue("surface",         _tokens.surface);
        s.setValue("surfaceAlt",      _tokens.surfaceAlt);
        s.setValue("hover",           _tokens.hover);
        s.setValue("chipBg",          _tokens.chipBg);
        s.setValue("text",            _tokens.text);
        s.setValue("textStrong",      _tokens.textStrong);
        s.setValue("textMuted",       _tokens.textMuted);
        s.setValue("accentText",      _tokens.accentText);
        s.setValue("accentTextAlt",   _tokens.accentTextAlt);
        s.setValue("onAccent",        _tokens.onAccent);
        s.setValue("border",          _tokens.border);
        s.setValue("scrollbar",       _tokens.scrollbar);
        s.setValue("accentFill",      _tokens.accentFill);
        s.setValue("accentFillHover", _tokens.accentFillHover);
        s.setValue("danger",          _tokens.danger);
        s.setValue("success",         _tokens.success);

        s.endGroup();
        s.sync();                   // // 马上写文件
    }

// ---------------------------------------------------------------- 持久化

// ---------------------------------------------------------------- QSS 生成
//先拿到qss模板，即带有部分@XXXX@的qss，然后将@XXXX@ 替换为 _tokens里的对应项内容
QString ThemeManager::buildQss(){
    QFile f(QString::fromUtf8(kTemplatePath));
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "[theme] 打不开模板:" << kTemplatePath;
        return QString();
    }
    // 模板里有中文注释，必须 fromUtf8（原来的 fromLatin1 只是因为 QSS 主体是 ASCII）
    QString qss = QString::fromUtf8(f.readAll());
    /*
        f.readAll() 的产物：
            返回的是 QByteArray。它本质上是一个纯粹的 8 位字节容器，完全不关心编码，里面装的只是 0x00-0xFF 的二进制数据10。
        QString 的内部机制：
            Qt 的 QString 内部统一使用 UTF-16（16位宽字符）来存储 Unicode 字符
        为什么需要转换：
            由于底层的内存表征完全不同，必须通过一个明确的“解码器”告诉 Qt：“请按照 UTF-8 的编码规则，把这些 8 位的字节流重新组装成 16 位的 UTF-16 字符串。”
                fromUtf8() 就是执行这个解码过程的显式指令。
        如果省略 fromUtf8()，直接写 QString qss = f.readAll();，编译器会触发隐式转换。
            在 Qt 的底层设计中，这种隐式转换默认会调用 fromLatin1()（单字节编码）1017。Latin-1 仅支持 ASCII 和西欧字符，
                一旦你的 QSS 文件中包含任何非 ASCII 字符（例如中文注释、特殊的 Unicode 符号），就会被错误解析，直接导致乱码或数据丢失
    */
    f.close();

    const auto pairs = tokenPairs(_tokens);
    for (const auto& p : pairs)
        qss.replace(p.first, p.second);

    // 兜底：模板里出现没定义的令牌时，这里能直接看到
    static const QRegularExpression leftover(QStringLiteral("@[A-Za-z][A-Za-z0-9]*@"));
    auto it = leftover.globalMatch(qss);
    while (it.hasNext())
        qWarning() << "[theme] 未定义令牌:" << it.next().captured();

    if (hasWallpaper())
        qss += wallpaperOverride();

    return qss;
}

void ThemeManager::apply(){
    if (_dirty) {
        _cachedQss = buildQss();
        _dirty = _cachedQss.isEmpty();   // 只有成功才清脏，失败下次还能重试
    }
    // 静态销毁期 qApp 可能已经没了
    if (qApp)
        qApp->setStyleSheet(_cachedQss);

    emit sigChanged();
}

const ThemeTokens& ThemeManager::tokens()const{
    return _tokens;
}
// ---------------------------------------------------------------- QSS 生成
// ---------------------------------------------------------------- 改主题
void ThemeManager::setTokens(const ThemeTokens& t){
    _tokens = t;
    _dirty = true;
}

//目前我认为它主要是提高亮度？？？？？
void ThemeManager::setAccent(const QColor& base){
    setTokens(tokensForAccent(base));
}

void ThemeManager::resetToDefault(){
    setTokens(defaultTokens());
}
// ---------------------------------------------------------------- 改主题
bool ThemeManager::hasWallpaper()const{
    return !_wallpaperPath.isEmpty();
}

QString ThemeManager::wallpaperPath()const{
    return _wallpaperPath;
}

int ThemeManager::wallpaperBlur()const{
    return _wallpaperBlur;
}

WallpaperLayoutMode ThemeManager::wallpaperMode()const{
    return _wallpaperMode;
}

int ThemeManager::wallpaperOpacity()const{
    return _wallpaperOpacity;
}

int ThemeManager::wallpaperScrimAlpha()const{
    return _wallpaperScrimAlpha;
}

void ThemeManager::setWallpaperPath(const QString& path){
    _wallpaperPath = path;
    _dirty = true;
    emit sigChanged();
}
void ThemeManager::setWallpaperBlur(int opacity){
    _wallpaperBlur = qBound(0, opacity, 40);
    _dirty = true;
    emit sigChanged();
}
void ThemeManager::setWallpaperMode(WallpaperLayoutMode mode){
    _wallpaperMode = mode;
    _dirty = true;
    emit sigChanged();
}
void ThemeManager::setWallpaperOpacity(int percent){
    _wallpaperOpacity = qBound(0, percent, 100);
    _dirty = true;
    emit sigChanged();
}
void ThemeManager::setWallpaperScrimAlpha(int alpha){
    _wallpaperScrimAlpha = qBound(0, alpha, 255);
    _dirty = true;
    emit sigChanged();
}

QString ThemeManager::wallpaperOverride()const{
    auto rgba = [](const QString& hex, int a) {
        const QColor c(hex);
        return QString("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(a);
    };
    const QString surf = tokens().surface;   // #ffffff
    const QString bg   = tokens().bg;        // #f7f7f8

    QString s;
    s += "\n/* ===== wallpaper override ===== */\n";
        //其余的side_bar, list, chat_bg 是透明的，可以看到wallpaper
    s += "#side_bar{background:rgba(46,46,46,170);}\n";
    s += "#chat_user_list,#search_list,#con_user_list{background:transparent;}\n";
    s += "ChatPage,#chat_bg,#chat_data_list{background:transparent;}\n";
        //这三个组件不是transparent
    s += QString("#tool_wid,#send_wid{background:%1;}\n").arg(rgba(surf, 200));
    s += QString("#chat_edit{background:%1;}\n").arg(rgba(surf, 220));
    s += QString("#search_wid{background:%1;}\n").arg(rgba(bg, 160));
    s += "#search_edit{background:transparent;}\n";   // QLineEdit 默认白底，必须显式改
    return s;
}
