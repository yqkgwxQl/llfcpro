#ifndef LLFC_THEME_TOKENS_H
#define LLFC_THEME_TOKENS_H

#include <QString>
#include <QColor>
#include <QVector>
#include <QPair>

// 主题令牌表。
//   新增一个令牌只需三步：① 本结构体加字段 ② defaultTokens() 给默认值
//   ③ tokenSlots() 加一行。load/save/buildQss 全部自动覆盖，不用再逐处枚举。
struct ThemeTokens {
    // —— 底 ——
    QString sideBar;          // 最左侧深色竖栏
    QString bg;               // 页面底色（列表背后）
    QString surface;          // 白色卡片：搜索/输入/弹窗
    QString surfaceAlt;       // 次级面板：聊天区底/按钮底
    QString hover;            // 浅灰块底（分组标题带、搜索无结果提示）
    QString itemSelected;     // 列表项选中
    QString itemHover;        // 列表项悬停
    QString chipBg;           // 分组标题/标签的灰底
    QString btnBgHover;       // 胶囊按钮悬停
    QString btnBgPress;       // 胶囊按钮按下
    // —— 字 ——
    QString text;
    QString textStrong;
    QString textMuted;
    QString accentText;
    QString accentTextAlt;
    QString onAccent;
    // —— 线 ——
    QString border;           // 浅边框（搜索框、对话框外框）
    QString borderLight;      // 更浅的分隔线（工具栏下边、条目分隔）
    QString borderStrong;     // 较明显的分隔线（申请条目底部 2px）
    QString scrollbar;
    // —— 强调 ——
    QString accentFill;
    QString accentFillHover;
    QString danger;
    QString success;
};

// 令牌名 ↔ 结构体字段，加载/保存/替换共用这一张表
struct TokenSlot {
    const char* key;
    QString ThemeTokens::*member;
};

inline const QVector<TokenSlot>& tokenSlots()
{
    // 注意：变量不能叫 slots —— Qt 把 slots 定义成了宏（Q_SLOTS），会编译报错
    static const QVector<TokenSlot> kTable = {
        // —— 底 ——
        {"sideBar",          &ThemeTokens::sideBar},
        {"bg",               &ThemeTokens::bg},
        {"surface",          &ThemeTokens::surface},
        {"surfaceAlt",       &ThemeTokens::surfaceAlt},
        {"hover",            &ThemeTokens::hover},
        {"itemSelected",     &ThemeTokens::itemSelected},
        {"itemHover",        &ThemeTokens::itemHover},
        {"chipBg",           &ThemeTokens::chipBg},
        {"btnBgHover",       &ThemeTokens::btnBgHover},
        {"btnBgPress",       &ThemeTokens::btnBgPress},
        // —— 字 ——
        {"text",             &ThemeTokens::text},
        {"textStrong",       &ThemeTokens::textStrong},
        {"textMuted",        &ThemeTokens::textMuted},
        {"accentText",       &ThemeTokens::accentText},
        {"accentTextAlt",    &ThemeTokens::accentTextAlt},
        {"onAccent",         &ThemeTokens::onAccent},
        // —— 线 ——
        {"border",           &ThemeTokens::border},
        {"borderLight",      &ThemeTokens::borderLight},
        {"borderStrong",     &ThemeTokens::borderStrong},
        {"scrollbar",        &ThemeTokens::scrollbar},
        // —— 强调 ——
        {"accentFill",       &ThemeTokens::accentFill},
        {"accentFillHover",  &ThemeTokens::accentFillHover},
        {"danger",           &ThemeTokens::danger},
        {"success",          &ThemeTokens::success},
    };
    return kTable;
}

// 1) 默认值 = 现在 QSS 里的原色（所以整理完界面必须基本一模一样）
inline ThemeTokens defaultTokens()
{
    ThemeTokens t;
    // —— 底 ——
    t.sideBar          = "#2e2e2e";
    t.bg               = "#f7f7f8";
    t.surface          = "#ffffff";
    t.surfaceAlt       = "#f0f0f0";
    t.hover            = "#eaeaea";
    t.itemSelected     = "#d3d7d4";
    t.itemHover        = "#cecfd0";
    t.chipBg           = "#e1e1e1";
    t.btnBgHover       = "#d2d2d2";
    t.btnBgPress       = "#c6c6c6";
    // —— 字 ——
    t.text             = "#2e2f30";
    t.textStrong       = "#1e1e1e";
    t.textMuted        = "#a2a2a2";
    t.accentText       = "#2cb46e";
    t.accentTextAlt    = "#48bf56";
    t.onAccent         = "#ffffff";
    // —— 线 ——
    t.border           = "#f1f1f1";
    t.borderLight      = "#eaeaea";
    t.borderStrong     = "#dbd9d9";
    t.scrollbar        = "#adaba9";
    // —— 强调 ——
    t.accentFill       = "#07c160";
    t.accentFillHover  = "#3bbb4b";
    t.danger           = "#fa5151";
    t.success          = "#07c160";
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

// 3) 生成"令牌名 → 值"的列表，buildQss 用（@key@ 由 key 拼出来，不另写一遍）
inline QVector<QPair<QString, QString>> tokenPairs(const ThemeTokens& t)
{
    QVector<QPair<QString, QString>> out;
    const auto& table = tokenSlots();
    out.reserve(table.size());
    for (const TokenSlot& s : table) {
        out.append({ QString("@%1@").arg(QString::fromLatin1(s.key)),
                     t.*(s.member) });
    }
    return out;
}

#endif // LLFC_THEME_TOKENS_H
