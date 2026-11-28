#include "ThemeManager.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QImageReader>

namespace {
    const char* kTemplatePath = ":/style/stylesheet.tokened.qss";
    const char* kIniGroup     = "theme";

// 壁纸模式下"每个区域"的底色 + 透明度。
// 选择器规则：必须和主表 stylesheet.tokened.qss 里的特异性持平或更高，
// 否则会被主表压住（QSS 按特异性比较，不是按书写顺序）。
const QVector<SurfaceSpec>& surfaceSpecs(){
    static const QVector<SurfaceSpec> kTable = {
        // key        选择器（必须带祖先 id）                                                    colorToken    wpAlpha  tintAlpha    blur
        {"sideBar",   {"#side_bar"},                                                             "sideBar",   255,       0,       -1},

        // 列表列：三个列表都是 chat_user_wid 的子控件，和主表逐字对齐
        {"list",      {"#chat_user_wid #chat_user_list",
                       "#chat_user_wid #search_list",
                       "#chat_user_wid #con_user_list"},                                         "bg",        255,       0,       -1},

        // 聊天页：只给页面根铺色（chat_bg 在主表里已是 transparent，会跟着根走）
        {"chat",      {"#chat_page"},                                                            "surface",    255,       0,      -1},

        {"toolbar",   {"#tool_wid"},                                                             "surface",    255,     200,      -1},
        {"input",     {"#send_wid"},                                                             "surface",    255,     180,      -1},

        // 输入框在 chat_data_wid 里；带祖先是为了压住主表的 #chat_edit
        {"edit",      {"#chat_data_wid #chat_edit"},                                             "surface",    255,     180,      -1},

        {"search",    {"#search_wid"},                                                           "bg",         255,   0,          -1},

        // 新的朋友页：friend_apply_page 是页面根，friend_apply_wid 是标题栏容器，
        // 后两个和主表选择器逐字一致
        {"applyPage", {"#friend_apply_page",
                       "#friend_apply_wid",
                       "ApplyFriendPage #apply_friend_list",
                       "#friend_apply_wid #friend_apply_lb"},                                    "surfaceAlt", 255,    0,        -1},

        // 申请好友对话框是独立顶层窗口：半透明会透出桌面，这里保持不透明
        {"applyDlg",  {"#ApplyFriend",
                      "#ApplyFriend #scrollArea",
                      "#ApplyFriend #scrollcontent"},                                           "surface",     255,    255,       -1},
        };
        return kTable;
    }

}
// ---------------------------------------------------------------- 持久化
    void ThemeManager::load(){

        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(dir);                       // 首次不存在，必须建

        QSettings s(dir + QStringLiteral("/theme.ini"), QSettings::IniFormat);
        s.beginGroup(QString::fromUtf8(kIniGroup));

        ThemeTokens t = defaultTokens();
        // 遍历令牌表，不再逐个字段手写：加令牌只改 ThemeTokens.h
        for (const TokenSlot& slot : tokenSlots()) {
            const QString key = QString::fromLatin1(slot.key);
            t.*(slot.member) = s.value(key, t.*(slot.member)).toString();   // 读不到的键保持默认
        }
        /*
        原先是这样写的：
            t.sideBar          = s.value("sideBar",          t.sideBar).toString();
            t.bg               = s.value("bg",               t.bg).toString();
                ...
                ...
                ...
        s.value("sideBar",t.sideBar)
            从配置里读取键 "sideBar"
                如果读到了，就用配置里的值
                    如果没读到，就用默认值 t.sideBar
        */

        _surfaceWpAlphas.clear();
        _surfaceTintAlphas.clear();
        _surfaceBlurs.clear();
        for (const SurfaceSpec& spec : surfaceSpecs()) {
            const int wpAlpha = s.value("wpAlpha_" + spec.key, spec.wpAlpha).toInt();
            const int tintAlpha = s.value("tintAlpha_" + spec.key, spec.tintAlpha).toInt();
            const int blur = s.value("blur_" + spec.key, spec.blur).toInt();
            // const QColor color = s.value("color_" + spec.key, spec.colorToken).toInt();
            _surfaceWpAlphas[spec.key] = qBound(0, wpAlpha, 255);
            _surfaceTintAlphas[spec.key] = qBound(0, tintAlpha, 255);
            _surfaceBlurs[spec.key] = blur;
            // _surfaceColors[spec.key] =
        }
        _wallpaperPath   = s.value("wallpaperPath",   _wallpaperPath).toString();
        _wallpaperBlur   = qBound(0, s.value("wallpaperBlur", _wallpaperBlur).toInt(), 40);
        _wallpaperMode   = WallpaperLayoutMode(qBound(0, s.value("wallpaperMode", int(_wallpaperMode)).toInt(), 3));
        _wallpaperOpacity = qBound(0.0, s.value("wallpaperOpacity", _wallpaperOpacity).toDouble(), 1.0);
        _wallpaperScrimAlpha = qBound(0, s.value("wallpaperScrim", _wallpaperScrimAlpha).toInt(), 255);
        updateTone();

        s.endGroup();

        setTokens(t);                             // _dirty = true
    }

    void ThemeManager::save() const{
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(dir);

        QSettings s(dir + QStringLiteral("/theme.ini"), QSettings::IniFormat);  //这里并不会if文件不存在就立即生成
        s.beginGroup(QString::fromUtf8(kIniGroup));

        for (const TokenSlot& slot : tokenSlots())
            s.setValue(QString::fromLatin1(slot.key), _tokens.*(slot.member));

        for (const SurfaceSpec& spec : surfaceSpecs()){
            s.setValue(QStringLiteral("wpAlpha_") + spec.key, surfaceWpAlpha(spec.key));
            s.setValue(QStringLiteral("tintAlpha_") + spec.key, surfaceTintAlpha(spec.key));
            s.setValue(QStringLiteral("blur_") + spec.key, surfaceBlur(spec.key));
        }

        s.setValue("wallpaperPath",    _wallpaperPath);
        s.setValue("wallpaperBlur",    _wallpaperBlur);
        s.setValue("wallpaperMode",    static_cast<int>(_wallpaperMode));
        s.setValue("wallpaperOpacity", _wallpaperOpacity);
        s.setValue("wallpaperScrim",   _wallpaperScrimAlpha);

        s.endGroup();
        s.sync();                   //马上写文件
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
        //复制一份 QSS 字符串，然后删除其中所有 /* ... */ 形式的注释，包括跨行注释。
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
    emit sigChanged();
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

double ThemeManager::wallpaperOpacity()const{
    return _wallpaperOpacity;
}

int ThemeManager::wallpaperScrimAlpha()const{
    return _wallpaperScrimAlpha;
}

void ThemeManager::setWallpaperPath(const QString& path){
    if (_wallpaperPath == path)
        return;
    _wallpaperPath = path;
    updateTone();
    _dirty = true;
    emit sigChanged();
}
void ThemeManager::setWallpaperBlur(int blur){
    int newBlur = qBound(0, blur, 40);
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
void ThemeManager::setWallpaperOpacity(double opacity){
    const double newOpacity = qBound(0.0, opacity, 1.0);
    if(_wallpaperOpacity == newOpacity)
        return;
    _wallpaperOpacity = newOpacity;
    _dirty = true;
    emit sigChanged();
}
void ThemeManager::setWallpaperScrimAlpha(int alpha){
    int newAlpha = qBound(0, alpha, 255);
    if(_wallpaperScrimAlpha == newAlpha)
        return;
    _wallpaperScrimAlpha = newAlpha;
    _dirty = true;
    emit sigChanged();
}

QString ThemeManager::wallpaperOverride()const{
    auto rgba = [](const QString& hex, int a) {
        const QColor c(hex);
        return QString("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(a);
    };
    QString out;
    // 注意：选择器必须与主样式表 style/stylesheet.tokened.qss 里的「特异性」持平或更高。
        // 主表里这些控件的规则已带父容器作用域（2 个 id），override 只写 1 个 id 会被压住
        // —— QSS 按 CSS 特异性比较，跟书写顺序无关。
    out += "\n/* ===== wallpaper override ===== */\n";

    // ---- ① 区域底色：全部由表生成 ----
        // 这些region由 BackgroundWid 负责画，QSS 不能再铺色
    static const QSet<QString> kPaintedByBackground = {"sideBar", "list", "chat"};
    for (const SurfaceSpec& spec : surfaceSpecs()) {
        const QString color = resolveSurfaceColor(spec.colorToken);
            //不是指定区域内的组件，一般只添加遮罩不透明度
        const int tintAlpha = surfaceTintAlpha(spec.key);
        const QString bg = (tintAlpha == 0) ? QStringLiteral("transparent") : rgba(color, tintAlpha);
        const QString selectors = spec.selectors.join(QLatin1Char(','));
        if (kPaintedByBackground.contains(spec.key))
            out += QString("%1{background:transparent;}\n").arg(selectors);
        else
            out += QString("%1{background:%2;}\n").arg(selectors, bg);
    }

    // ---- ② 不适合进表的固定例外 ----
    // 搜索框是输入控件，必须清楚可读，用实心白
    out += QString("#search_wid #search_edit{background:%1;}\n").arg(tokens().surface);
    // 分组标题条：整条透明，只留一条分隔线（线色在 ③ 里按明暗给）
    out += QString("#GroupTipItem{background:transparent;}\n");

    // ---- ③ 随壁纸明暗切换文字 / 线条颜色 ----
    const QString line_dark("rgba(255,255,255,28)");
    const QString line_light("rgba(0,0,0,18)");
    constexpr int kDarkThreshold = 140;   // <140 认为是暗底 → 用白字
        //阈值 128 落在"两边都不好看"的灰区 ————> 阈值从 128 挪到 140：宁可早一点转白字（白字在灰底上的观感通常更好）
    if (_toneTitle >= 0) {
        const QString c = (_toneTitle < kDarkThreshold) ? "#ffffff" : tokens().text;
        out += QString("#title_wid #title_lb{color:%1;}\n").arg(c);
        out += QString("#friend_apply_wid #friend_apply_lb{color:%1;}\n").arg(c);
    }
    if (_toneBody >= 0) {
        const bool dark = (_toneBody < kDarkThreshold);
        const QString line = dark ? line_dark : line_light;

        out += QString("#chat_user_name{color:%1;}\n").arg(dark ? "rgba(255,255,255,200)" : tokens().text);

        out += QString("ChatUserWid #user_name_lb{color:%1;}\n").arg(dark ? "rgba(255,255,255,200)" : tokens().text);
        out += QString("ConUserItem #user_name_lb{color:%1;}\n").arg(dark ? "rgba(255,255,255,200)" : tokens().text);

        out += QString("ConUserItem#con_user_item{border-bottom:1px solid %1;}\n").arg(line);
        /*
            写法                           翻译成人话                                     检查的是
            #con_user_item              名字叫 con_user_item 的那个控件                自身的 objectName
            ConUserItem                 类是 ConUserItem 的那个控件	                 自身的 class
            ConUserItem#con_user_item	类 ConUserItem 且名字 con_user_item 的控件	     自身两个属性：class + objectName
            ConUserItem #user_name_lb	在 ConUserItem 里面找名字 user_name_lb 的控件	名字那个控件的祖先链里有没有 ConUserItem

        关键差别就一句：
            有空格 = 在“里面”找（不含自己）；没空格 = 检查“自己”。
        */
        out += QString("#GroupTipItem{border-bottom:1px solid %1;}\n").arg(line);
        out += QString("#GroupTipItem QLabel{color:%1;}\n").arg(dark ? "rgba(255,255,255,150)" : tokens().text);

        out += QString("ApplyFriendItem #user_name_lb{color:%1;}\n").arg(dark ? "rgba(255,255,255,220)" : tokens().textStrong);
        out += QString("ApplyFriendItem #user_chat_lb{color:%1;}\n").arg(dark ? "rgba(255,255,255,170)" : tokens().text);
        out += QString("ApplyFriendItem #already_add_lb{color:%1;}\n").arg(dark ? "rgba(255,255,255,150)" : tokens().text);
        out += QString("ApplyFriendPage #apply_friend_list{border-left:1px solid %1;}\n").arg(line);

        out += QString("#title_wid{border-bottom:1px solid %1;}\n").arg(line);
        out += QString("#friend_apply_wid #friend_apply_lb{border-bottom:1px solid %1;}\n").arg(line);
        out += QString("#chat_divider_wid #chat_divider_lb{background-color:%1;}\n").arg(line);
    }

    return out;
}

int ThemeManager::surfaceWpAlpha(const QString& key)const{
    if (_surfaceWpAlphas.contains(key))
        return _surfaceWpAlphas.value(key);

    for (const SurfaceSpec& spec : surfaceSpecs())
        if (spec.key == key)
            return spec.wpAlpha;          // 没设置过 → 用表里的默认值

    qWarning() << "[theme] 未知的区域 key:" << key;
    return 255;
}

void ThemeManager::setSurfaceWpAlpha(const QString& key, int alpha){
    if(surfaceWpAlpha(key) == alpha)
        return;

    _surfaceWpAlphas[key] = alpha;
    _dirty = true;
    emit sigChanged();
}

int ThemeManager::surfaceTintAlpha(const QString& key)const{
    if (_surfaceTintAlphas.contains(key))
        return _surfaceTintAlphas.value(key);

    for (const SurfaceSpec& spec : surfaceSpecs())
        if (spec.key == key)
            return spec.tintAlpha;          // 没设置过 → 用表里的默认值

    qWarning() << "[theme] 未知的区域 key:" << key;
    return 255;
}

void ThemeManager::setSurfaceTintAlpha(const QString& key, int alpha){
    if(surfaceTintAlpha(key) == alpha)
        return;
    _surfaceTintAlphas[key] = alpha;
    _dirty = true;
    emit sigChanged();
}

int ThemeManager::realRegionWpAlpha(const QString& key)const{
    const int wp = surfaceWpAlpha(key);                 // 0~255
    return static_cast<int>(wallpaperOpacity() * wp);               // 0~100% × 系数
}

int ThemeManager::surfaceBlur(const QString& key)const{
    if(_surfaceBlurs.contains(key))
        return _surfaceBlurs.value(key);
    for(const auto& temp : surfaceSpecs()){
        if(key == temp.key)
            return temp.blur;
    }
    return -1;
}

void ThemeManager::setSurfaceBlur(const QString& key, int blur){
    if(surfaceBlur(key) == blur)
        return;
    _surfaceBlurs[key] = blur;
    _dirty = true;
    emit sigChanged();
}

QColor ThemeManager::surfaceColor(const QString& key)const{
    for(const auto& temp : surfaceSpecs()){
        if(temp.key == key)
            return resolveSurfaceColor(temp.colorToken);
    }
    return QColor(tokens().surface);
}

void ThemeManager::setSurfaceColor(const QString& key, const QColor& color){
    if(surfaceColor(key) == color)
        return;
    _surfaceColors[key] = color;
    _dirty = true;
    emit sigChanged();
}

QStringList ThemeManager::surfaceKeys() const{
    QStringList keys;
    for (const SurfaceSpec& spec : surfaceSpecs())
        keys << spec.key;
    return keys;
}

QImage ThemeManager::getScaledImage(const QString &path, QSize size)const{
    QImageReader reader(path);              // 比 QImage(path) 多一层控制
    reader.setScaledSize(size);    // 解码时就缩小（JPEG 走 DCT 缩放，很快）
    const QImage img = reader.read();
    if (img.isNull())
        return QImage();
    return img;
}

int ThemeManager::computeWallpaperTone(const QImage& img, double fromRatio, double toRatio)const{
    if (img.isNull())
        return -1;
    const int h = img.height();
    const int from = qBound(0,          static_cast<int>(h * fromRatio), h - 1);
    const int to   = qBound(from + 1,   static_cast<int>(h * toRatio),   h);
    double sum = 0;
    for (int y = from; y < to; ++y)
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            //按照人眼能够感知亮度的比例进行对应权重的加
            sum += 0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue();
        }
    return int(sum / (img.width() * (to - from))); // 除以实际总像素数
}

void ThemeManager::updateTone(){
    if(!_wallpaperPath.isEmpty()){
        const QImage img = getScaledImage(_wallpaperPath, QSize(64, 64));
        _toneTitle = computeWallpaperTone(img, 0.0,   1.0 / 6);
        _toneBody  = computeWallpaperTone(img, 1.0 / 6, 1.0);
    }else{
        _toneTitle = _toneBody = -1;
    }
}

QString ThemeManager::resolveSurfaceColor(const QString& token) const{
    if (token == QLatin1String("sideBar"))    return tokens().sideBar;
    if (token == QLatin1String("bg"))         return tokens().bg;
    if (token == QLatin1String("surfaceAlt")) return tokens().surfaceAlt;
    if (token == QLatin1String("surface"))    return tokens().surface;

    qWarning() << "[theme] 未知的 colorToken:" << token;   // 写错时能立刻看到
    return tokens().surface;
}
