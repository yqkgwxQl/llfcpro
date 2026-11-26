#ifndef LLFC_THEME_MANAGER_H
#define LLFC_THEME_MANAGER_H

#include <QObject>
#include <QString>
#include <QColor>

#include "singleton.h"
#include "ThemeTokens.h"

// 主题管理器：负责"令牌 → QSS 字符串 → 应用到整个应用"
// 单例来自项目里的 Singleton<T>，用法与 TcpMgr / HttpMgr 一致：
//     ThemeManager::GetInstance()->load();
class ThemeManager : public QObject, public Singleton<ThemeManager>
{
    Q_OBJECT
    friend class Singleton<ThemeManager>;

    struct SurfaceSpec {
        QString     key;        // "list" / "chat" / "input" ... ————> 配置的每一组的名字
        QStringList selectors;  // 对应的 QSS 选择器
        QString     colorToken; // "surface" / "bg" / "sideBar" ————> 底色从哪个令牌取
        int         alpha;      // 0~255
    };
public:
    ~ThemeManager() = default;

    void load();        // theme.ini → _tokens（读不到就用默认值）
    void save() const;  // _tokens → theme.ini


    QString buildQss(); // 模板 + 令牌替换（public 方便调试/测试）
    void apply();       // 脏了才重建 QSS，然后 qApp->setStyleSheet()


    const ThemeTokens& tokens()const;
    void setTokens(const ThemeTokens& t);   // 置脏，不立即应用
    void setAccent(const QColor& base);     // = setTokens(tokensForAccent(base))
    void resetToDefault();

    bool hasWallpaper()const;
    QString wallpaperPath()const;
    int wallpaperBlur()const;
    WallpaperLayoutMode wallpaperMode()const;
    int wallpaperOpacity()const;
    int wallpaperScrimAlpha()const;
    void setWallpaperPath(const QString& path);
    void setWallpaperBlur(int blur);
    void setWallpaperMode(WallpaperLayoutMode mode);
    void setWallpaperOpacity(int opacity);
    void setWallpaperScrimAlpha(int alpha);

    //如果有壁纸的话，让一些组件变透明，防止壁纸看不见
    QString wallpaperOverride()const;
signals:
    void sigChanged();  // 壁纸层 监听它重绘

private:
    ThemeManager() = default;

    ThemeTokens _tokens = defaultTokens();
    QString     _cachedQss;
    bool        _dirty = true;

    QString _wallpaperPath = "";
    int _wallpaperBlur = 0;
    WallpaperLayoutMode _wallpaperMode = WallpaperLayoutMode::Fill;
    int _wallpaperOpacity = 100;
    int _wallpaperScrimAlpha = 0;

    int _toneTitle  = -1;    // 0..255，-1 表示无壁纸/算不出来  ————> 用来计算title_wid处是偏暗还是偏亮，如果偏暗：黑底白字；如果偏亮：白底黑字 保证能够看到对端用户的名字
    int _toneBody = -1;
    int computeWallpaperTone(const QString& path, double fromRatio, double toRatio)const;
};

#endif // LLFC_THEME_MANAGER_H