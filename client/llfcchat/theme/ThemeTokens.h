#ifndef LLFC_THEME_TOKENS_H
#define LLFC_THEME_TOKENS_H

#include <QString>
#include <QColor>
#include <QVector>
#include <QPair>

struct ThemeTokens {
    // —— 底 ——
    QString sideBar;          // 最左侧深色竖栏
    QString bg;               // 页面底色（列表背后）
    QString surface;          // 白色卡片：搜索/输入/弹窗
    QString surfaceAlt;       // 次级面板：聊天区底/按钮底
    QString hover;            // 列表项悬停
    QString chipBg;           // 分组标题/标签的灰底
    // —— 字 ——
    QString text;
    QString textStrong;
    QString textMuted;
    QString accentText;
    QString accentTextAlt;
    QString onAccent;
    // —— 线 & 强调 ——
    QString border;
    QString scrollbar;
    QString accentFill;
    QString accentFillHover;
    QString danger;
    QString success;
};

// 1) 默认值 = 现在 QSS 里的原色（所以 M2 做完界面必须一模一样）
inline ThemeTokens defaultTokens()
{
    ThemeTokens t;
    t.sideBar = "#2e2e2e";        t.bg = "#f7f7f8";
    t.surface = "#ffffff";        t.surfaceAlt = "#f0f0f0";
    t.hover = "#eaeaea";          t.chipBg = "#e1e1e1";
    t.text = "#2e2f30";           t.textStrong = "#1e1e1e";
    t.textMuted = "#a2a2a2";
    t.accentText = "#2cb46e";     t.accentTextAlt = "#48bf56";
    t.onAccent = "#ffffff";
    t.border = "#f1f1f1";         t.scrollbar = "#adaba9";
    t.accentFill = "#07c160";     t.accentFillHover = "#3bbb4b";
    t.danger = "#fa5151";         t.success = "#07c160";
    return t;
}

// 2) 由"主色"推导整套（M5 的色轮会调它）
inline ThemeTokens tokensForAccent(const QColor& base)
{
    ThemeTokens t = defaultTokens();
    const int h = qMax(0, base.hslHue());
    const int s = base.hslSaturation();
    const int l = base.lightness();

    t.accentText      = base.name();
    //qBound(min, val, max)  逻辑：如果 val < min，返回 min；如果 val > max，返回 max；否则返回 val 本身。
    //.name() 方法，将该 QColor 对象转换为 #RRGGBB 格式的字符串
    t.accentTextAlt   = QColor::fromHsl(h, s, qBound(100, l + 20, 240)).name();
    t.accentFill      = QColor::fromHsl(h, s, qBound(70, l - 10, 200)).name();
    t.accentFillHover = QColor::fromHsl(h, s, qBound(80, l, 210)).name();
    // 主色偏亮 → 用深字；偏暗 → 用白字
    t.onAccent        = (l > 160) ? "#1e1e1e" : "#ffffff";
    return t;
}

// 3) 生成"令牌名 → 值"的列表，buildQss 用
inline QVector<QPair<QString, QString>> tokenPairs(const ThemeTokens& t)
{
    return {
            {"@sideBar@",         t.sideBar},
            {"@bg@",              t.bg},
            {"@surface@",         t.surface},
            {"@surfaceAlt@",      t.surfaceAlt},
            {"@hover@",           t.hover},
            {"@chipBg@",          t.chipBg},
            {"@text@",            t.text},
            {"@textStrong@",      t.textStrong},
            {"@textMuted@",       t.textMuted},
            {"@accentText@",      t.accentText},
            {"@accentTextAlt@",   t.accentTextAlt},
            {"@onAccent@",        t.onAccent},
            {"@border@",          t.border},
            {"@scrollbar@",       t.scrollbar},
            {"@accentFill@",      t.accentFill},
            {"@accentFillHover@", t.accentFillHover},
            {"@danger@",          t.danger},
            {"@success@",         t.success},
            };
}

#endif // LLFC_THEME_TOKENS_H