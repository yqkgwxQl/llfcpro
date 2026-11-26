#include "ThemeManager.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QImageReader>

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
        // 遍历令牌表，不再逐个字段手写：加令牌只改 ThemeTokens.h
        for (const TokenSlot& slot : tokenSlots()) {
            const QString key = QString::fromLatin1(slot.key);
            t.*(slot.member) = s.value(key, t.*(slot.member)).toString();
        }

        s.endGroup();

        setTokens(t);                             // _dirty = true
    }

    void ThemeManager::save() const{
        const QString dir =
            QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(dir);

        QSettings s(dir + QStringLiteral("/theme.ini"), QSettings::IniFormat);  //这里并不会if文件不存在就立即生成
        s.beginGroup(QString::fromUtf8(kIniGroup));

        for (const TokenSlot& slot : tokenSlots())
            s.setValue(QString::fromLatin1(slot.key), _tokens.*(slot.member));

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
    // 先剥掉 /* */ 注释再查，否则注释里举例写的 @xxx@ 会被误报
    QString bare = qss;
    static const QRegularExpression commentRe(
        QStringLiteral("/\\*.*?\\*/"), QRegularExpression::DotMatchesEverythingOption);
    bare.remove(commentRe);

    static const QRegularExpression leftover(QStringLiteral("@[A-Za-z][A-Za-z0-9]*@"));
    auto it = leftover.globalMatch(bare);
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
    if (_wallpaperPath == path)
        return;
    _wallpaperPath = path;
    if(path.isEmpty()){
        _toneTitle = _toneBody = -1;
    }else {
        _toneTitle = computeWallpaperTone(path, 0.0,    1.0/6);
        _toneBody  = computeWallpaperTone(path, 1.0/6,  1.0);
    }
    _dirty = true;
    emit sigChanged();
}
void ThemeManager::setWallpaperBlur(int blur){
    auto newBlur = qBound(0, blur, 40);
    if(_wallpaperBlur == newBlur)
        return;
    _wallpaperBlur = newBlur;
    _dirty = true;
    emit sigChanged();
}
void ThemeManager::setWallpaperMode(WallpaperLayoutMode mode){
    if(_wallpaperMode == mode)
        return;
    _wallpaperMode = mode;
    _dirty = true;
    emit sigChanged();
}
void ThemeManager::setWallpaperOpacity(int percent){
    if(_wallpaperOpacity == percent)
        return;
    _wallpaperOpacity = qBound(0, percent, 100);
    _dirty = true;
    emit sigChanged();
}
void ThemeManager::setWallpaperScrimAlpha(int alpha){
    if(_wallpaperScrimAlpha == alpha)
        return;
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
    s += QString("#side_bar{background:%1;}\n").arg(rgba(tokens().sideBar, 170));
    // 注意：选择器必须与主样式表 style/stylesheet.tokened.qss 里的「特异性」持平或更高。
    // 主表里这些控件的规则已带父容器作用域（2 个 id），override 只写 1 个 id 会被压住
    // —— QSS 按 CSS 特异性比较，跟书写顺序无关。曾经就因此让列表/输入框变回不透明。
    s += "#chat_user_wid #chat_user_list,#chat_user_wid #search_list,#chat_user_wid #con_user_list{background:transparent;}\n";
    s += "ChatPage,#chat_bg,#chat_data_list{background:transparent;}\n";
        //这三个组件不是transparent，用半透明白让壁纸透一点
    s += QString("#tool_wid,#send_wid{background:%1;}\n").arg(rgba(surf, 200));
    s += QString("#chat_data_wid #chat_edit{background:%1;}\n").arg(rgba(surf, 220));
        //搜索栏：alpha 从 160 提到 220（160 时 63% 白叠在深色壁纸上会发灰）
    s += QString("#search_wid{background:%1;}\n").arg(rgba(bg, 220));
        //搜索框是输入控件，必须清楚可读 —— 用实心白，不再透明
    s += QString("#search_wid #search_edit{background:%1;}\n").arg(surf);
        //标题根据壁纸的亮暗进行黑白显示
    // 标题       阈值 128 落在"两边都不好看"的灰区 ————> 阈值从 128 挪到 140：宁可早一点转白字（白字在灰底上的观感通常更好）
    if (_toneTitle >= 0)
        s += QString("#title_wid #title_lb{color:%1;}\n")
                 .arg(_toneTitle < 140 ? "#ffffff" : tokens().text);

    // 发送者名字：暗→反白；亮→用 @text（不是 muted！）
    if (_toneBody >= 0)
        s += QString("#chat_user_name{color:%1;}\n")
                 .arg(_toneBody < 140 ? "rgba(255,255,255,200)" : tokens().text);
    return s;
}

int ThemeManager::computeWallpaperTone(const QString& path, double fromRatio, double toRatio)const{
    QImageReader reader(path);              // 比 QImage(path) 多一层控制
    reader.setScaledSize(QSize(64, 64));    // 解码时就缩小（JPEG 走 DCT 缩放，很快）
    const QImage img = reader.read();
    if (img.isNull()) return -1;

    const int from = static_cast<int>(qMax(1.0, img.height()*fromRatio));
    const int to = static_cast<int>(qMax(1.0, img.height()*toRatio));
    double sum = 0;
    for (int y = from; y < to; ++y)
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            //按照人眼能够感知亮度的比例进行对应权重的加
            sum += 0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue();
        }
    return int(sum / (img.width() * (to - from))); // 除以总像素数
}
