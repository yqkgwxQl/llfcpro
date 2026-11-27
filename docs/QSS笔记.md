# QSS 笔记

> 这份文档是 `client/llfcchat/style/stylesheet.tokened.qss` 的配套说明。
> 2026-11-26 整理 QSS 时，把原来塞在样式文件里的大段讲解搬到了这里，
> 让 `.qss` 只留「能一眼扫完的规则 + 一行注释」。

- 相关文件：`client/llfcchat/style/stylesheet.tokened.qss`、`client/llfcchat/theme/ThemeTokens.h`
- 相关文档：[改造记录.md](改造记录.md)（壁纸 / 主题的改造过程）

---

## 1. 选择器速查

| 写法 | 含义 | 例子 |
|---|---|---|
| `类名` | 按 C++ 类名匹配（含子类） | `ApplyFriendItem` |
| `#objectName` | 按 `objectName` 匹配 | `#side_bar` |
| `类名#objectName` | 两者都要满足 | `QDialog#LoginDialog` |
| `A B` | 后代（B 在 A 里面） | `#apply_wid QLabel` |
| `[prop='值']` | 动态属性（C++ 里 `setProperty` 设） | `#add_btn[state='hover']` |
| `::item:selected` | 子控件 `item` 处于 `selected` 状态 | `#chat_user_list::item:selected` |

优先级：`#id` > `类名` > 类型；带 `[属性]` 的再高一档。
所以 `#forgetLabel[state='hover']` 能盖住 `#forgetLabel[state='normal']`，不用靠书写顺序。

> **`#` 前面有没有空格是两回事**：
> `QDialog#LoginDialog` = 同一个控件既是 QDialog 又叫 LoginDialog；
> `QDialog #LoginDialog` = QDialog **里面**某个叫 LoginDialog 的控件。写错一个空格，语义完全相反。

### 1.1 为什么有的规则带父容器、有的不带

文件里两种写法并存，不是风格不统一，而是**按用途分的**：

```css
#tool_wid #emo_lb[state='hover'] { ... }    /* 带父容器：能看出它在工具栏里 */
#side_bar { ... }                            /* 裸写：它本身就是容器 */
```

**判断标准一句话**：把 `#id` 换成 `父容器 #id` 之后，选择器是"更好懂"还是"更啰嗦"？
前者加，后者不加。

| 情况 | 做法 | 例子 |
|---|---|---|
| 名字通用 + 只有一处宿主 | **加** | `#search_wid #add_btn`、`#tool_wid #emo_lb`、`AddUserItem #message_tip`、`FindSuccessDlg #name_lb` |
| 容器级控件本身 | 不加 | `#side_bar`、`#tool_wid`、`#scrollcontent`、`#apply_wid` |
| `.ui` 的根控件 | 不加 | `#LoginDialog`、`#ApplyFriend`、`#FindSuccessDlg` |
| 代码创建（QSS 里写不出父容器） | 不加 | `#chat_bg`、`#chat_area`、`#invalid_item`、`#new_friend_item` |
| 多宿主共用，故意一条覆盖多页 | **必须裸写** | `#errTip`、`#userLabel`、`#pwdLabel`、`#red_point` |

**最后一类最容易被误改**，举个具体的：

```css
/* 一条规则同时管 登录 / 注册 / 重置密码 三个对话框的校验提示 */
#errTip[state='err'] { color: @danger@; }
```

如果把它限定成 `#LoginDialog #errTip`，另外两个对话框的校验提示就**静默失去颜色**。
要保住行为只能拆成三条，反而更难维护 —— 所以这类必须保持裸写。

**代价也要知道**：加了父容器就等于给规则加了一个"控件搬家就静默失效"的条件。
QSS 不报错、不警告，表现是"某个控件突然没样式了"。缓解办法是：
加前缀时用的都是**紧邻的、语义明确的**容器（`#tool_wid` 而不是 `#ChatPage`），
这样重构时 `grep tool_wid` 能顺着找到规则。

**还有个连带影响：覆盖同一控件的别处，选择器必须同步加作用域。**
`ThemeManager::wallpaperOverride()` 是运行时追加到样式表末尾的一组规则，原本全裸写：

```cpp
s += "#chat_user_list,#search_list,#con_user_list{background:transparent;}\n";   // 1 个 id
```

主表加作用域后变成 2 个 id（`#chat_user_wid #chat_user_list`），
按 CSS 特异性比较 **override 输了**，于是"透出壁纸"整条失效 ——
2026-11-26 实际踩到：会话列表、搜索结果、联系人列表、输入框全变回不透明。
**规则：任何覆盖同一控件的规则，选择器要和主表保持同等特异性**（照抄主表的选择器最稳妥）。

### 1.2 自查：有没有"指向不存在的控件"的死规则

```powershell
# QSS 里的 #id 集合  −  所有 .ui 的 name 集合  −  代码里 setObjectName 的集合
# 差出来的就是死规则（本项目目前为 0）
```

2026-11-26 用这个方法查出并修掉两条：`#forgetLabel`（`.ui` 里实际叫 `forgetLable`）
和 `#scrollcontent #apply_lb`（实际叫 `apple_lb`），详见 §5.4。

### 1.3 文字颜色用在哪（39 条带文字的规则）

2026-11-27 做了一次全量梳理，并定了一条规矩：**凡是带文字的规则都必须显式写 `color`**，
不再依赖调色板默认值。梳理前有 8 条规则没写 `color`，其中 5 处本来就是黑底白字场景的误判来源
（见下面的对比度说明）。

| 令牌 | 默认值 | 用在哪 |
|---|---|---|
| `@text@` | `#2e2f30` | 会话标题 `title_lb`、分组标题 `GroupTipItem QLabel`、输入框（`chat_edit` / `name_ed` / `back_ed` / `lb_ed`）、两个标题（`apply_lb` / `friend_apply_lb`）、找好友弹窗的名字 `name_lb`、搜索结果提示 `message_tip`、申请/验证对话框的默认字色 `#ApplyFriend`、取消按钮 `cancel_btn` |
| `@textMuted@` | `#a2a2a2` | 消息发送者名字 `chat_user_name`、会话摘要 `user_chat_lb`、时间戳 `time_lb`、「已添加」`already_add_lb`、申请表单标签 `#apply_wid QLabel`、`tip_lb` |
| `@accentText@` | `#2cb46e` | 主操作按钮上的绿字：发送 / 确认（`send_btn` / `sure_btn`）、添加（`addBtn`） |
| `@textStrong@` | `#1e1e1e` | 登录 / 注册的字段名（`userLabel` / `pwdLabel`）、会话与联系人条目名（`user_name_lb`）、已选 chip、忘记密码常态 |
| `@onAccent@` | `#ffffff` | 绿底按钮上的白字：加好友 `add_friend_btn` |
| `@accentTextAlt@` | `#48bf56` | 亮一档的绿字：已选 chip、`FriendLabel` |
| `@success@` / `@danger@` | `#07c160` / `#fa5151` | `#errTip` 的通过 / 失败 |
| `rgb(42,112,241)` | 链接蓝 | `#forgetLabel` 悬停（单点色，不入令牌） |

**为什么"要读的文字"一律用 `@text@` 而不是 `@textMuted@`？** 对比度差很多：

| 组合 | 对比度 | WCAG 正文（≥4.5） |
|---|---|---|
| `#2e2f30` 在白底 `#ffffff` 上 | **13.41 : 1** | ✅ |
| `#a2a2a2` 在白底 `#ffffff` 上 | **2.55 : 1** | ❌ |
| `#a2a2a2` 在面板 `#f0f0f0` 上 | **2.24 : 1** | ❌ |

所以 `@textMuted@` 只留给"扫一眼即可"的时间戳 / 摘要 / 已添加；输入框、标题、名字这些都用 `@text@`。

> **一个很容易漏的坑**：令牌少写结尾的 `@` 就不会被替换。
> 2026-11-27 在 `#title_wid #title_lb` 上就出现过 `color: @text;`（少一个 `@`）——
> `buildQss()` 只替换 `@text@`，于是这条声明原样留在 QSS 里，Qt **静默丢弃**，又退回默认黑。
> 自查方法：搜 `@[A-Za-z]+(?!@)`，或者干脆看 `buildQss()` 的"未定义令牌"警告有没有响。

---

## 2. 一个列表条目的绘制顺序（为什么选中色会「消失」）

`QListWidget` 的每一行画三次，**后面的盖住前面的**：

```text
第 1 层：QListWidget 自己
    ┌────────────────────────┐
    │  background: #f7f7f8    │
    └────────────────────────┘

第 2 层：每个 item 的背景（QSS 的 ::item:selected / ::item:hover 画在这层）
    ┌────────────────────────┐
    │  item[0]: #f7f7f8       │  ← 普通
    ├────────────────────────┤
    │  item[1]: #d3d7d4       │  ← 选中！
    ├────────────────────────┤
    │  item[2]: #f7f7f8       │
    └────────────────────────┘

第 3 层：item 里的自定义 widget（setItemWidget 塞进去的 ChatUserWid）
    ┌────────────────────────┐
    │  ChatUserWid[0]: 无背景 │
    ├────────────────────────┤
    │  ChatUserWid[1]: 无背景 │  ← 一旦这里写了 background，
    ├────────────────────────┤     上面那层 #d3d7d4 就永远看不见
    │  ChatUserWid[2]: 无背景 │
    └────────────────────────┘
```

**结论**：条目内的自定义 widget（`ChatUserWid` / `ConUserItem` / `ApplyFriendItem`）
**不要**在自己的 QSS 里写 `background`，否则 `::item:selected` 的选中色被它整个盖住。

历史教训：曾经给 `ChatUserWid` 写了 `background-color: rgb(247,247,247)`，
结果是「点了没反应」——选中色一直在，只是被盖住了。那条规则现在已注释掉。

---

## 3. 滚动条：六个子控件 + 滑块长度

### 3.1 名字来自「滚动动作」，不是位置

Qt 的滚动条继承自 `QAbstractSlider`，它只认四种改变 value 的方式：

| 枚举 | 含义 | QSS 子控件 |
|---|---|---|
| `SliderSingleStepSub` | 单步 −1 | `::sub-line` |
| `SliderSingleStepAdd` | 单步 +1 | `::add-line` |
| `SliderPageStepSub` | 一整页 − | `::sub-page` |
| `SliderPageStepAdd` | 一整页 + | `::add-page` |

拆开看：`add` = 让 value **变大**，`sub` = 变小；`line` = **一行**（`singleStep`），`page` = **一屏**（`pageStep`）。

- 竖直滚动条：value 越大越靠下 → `add` = 下，`sub` = 上
- 水平滚动条：value 越大越靠右 → `add` = 右，`sub` = 左

所以规则名后面必须跟 `:vertical` / `:horizontal` —— 同一套名字，方向含义会翻转。
`QSlider` 用的是**同一套名字**，这套命名是给两者共用的，只能按"动作"命名而不能按"位置"命名。

### 3.2 竖直滚动条怎么分段

```text
       ┌──────────────────────┐
       │   sub-line (上箭头)   │ ← 单步按钮区域，点一下 −1 行
       ├──────────────────────┤
       │                      │
       │      sub-page        │ ← 滑块【上方】剩余轨道
       │   (点它 = 上翻一页)   │    点一下 −1 页
       │                      │
       ├──────────────────────┤
       │       handle         │ ← 滑块本体（可拖动）
       ├──────────────────────┤
       │      add-page        │ ← 滑块【下方】剩余轨道
       │   (点它 = 下翻一页)   │
       ├──────────────────────┤
       │  add-line (下箭头)    │ ← 单步按钮区域，点一下 +1 行
       └──────────────────────┘
```

注意 `sub-page` / `add-page` **不在两端**，而是"滑块之外的那部分轨道"，
长度随滑块上下移动而变化 —— 这就是它们叫 page 而不叫 top / bottom 的原因。

### 3.3 三层结构

一个箭头按钮其实是三层叠起来的：

| 层 | 选择器 | 管什么 |
|---|---|---|
| 区域 | `::sub-line` / `::add-line` | 这一块**空间**（占多大） |
| 按钮盒 | `::up-button` / `::down-button` | 按钮的**外框**（Windows 风格会画凸起） |
| 箭头图 | `::up-arrow` / `::down-arrow` | 里面那个**三角形** |

所以 `::sub-line { height: 0px }` 是把**区域本身**压成 0，后两层跟着一起消失 ——
这是最省事的写法，也是主流写法（微信、QQ 那种"只有一根细条"的滚动条）。

还有个 `::groove`（整条轨道的底），项目里没写，所以轨道底用的是
`QScrollBar:vertical { background: transparent }`。

### 3.4 为什么这四条都得写

不写就退回当前平台样式（Windows 的 `windowsvista`），会看到
「滑块是新的、上下却还挂着两个默认小方块 + 灰色渐变轨道」的拼接怪。
这是自定义滚动条最常见的一类翻车，本项目把这四条全部清掉了。

### 3.5 `height: 0` 和 `background: none` 不是一回事

| 写法 | 外观 | 点击区域 |
|---|---|---|
| `::add-page { background: none; }` | 看不见 | **仍在**，点空白处还能翻页 ✅ |
| `::add-page { height: 0px; }` | 看不见 | **被删掉**，点空白处没反应 ❌ |
| `::add-line { height: 0px; }` | 看不见 | 单步按钮本就多余，删掉无害 ✅ |

所以本项目：`page` 用 `background: none`（只脱衣服、保留功能），
`line` 用 `height: 0`（连空间一起不要）。

### 3.6 滑块长度是按比例算的

```text
滑块长度 = 轨道长度 × 可视比例
可视比例 = 视口高度 / 内容总高度
```

| 内容比视口高 | 可视比例 | 滑块 |
|---|---|---|
| 刚好一屏 | 100% | **= 整条轨道**：上下没有 page 区域，也没有可移动的余地 |
| 2 屏 | 50% | 半条 |
| 10 屏 | 10% | 很短，上下露出大块 page 区域 |

同一份内容，滚动条**策略**不同，画出来的结果也不同（`AsNeeded` 是 Qt 的默认策略，
`AlwaysOff` 则永不显示）：

**范围 > 0（内容超过一屏）—— 两种策略没有区别**

```text
AlwaysOn                        AsNeeded
┌───────────┐                   ┌───────────┐
│ item1     │                   │ item1     │
│ item2     │                   │ item2     │
│ ▓▓▓▓▓     │ ← 滑块             │ ▓▓▓▓▓     │ ← 滑块
│ item3     │                   │ item3     │
│ item4     │                   │ item4     │
└───────────┘                   └───────────┘
              完全一样，没有区别
```

**范围 == 0（内容不满一屏）—— 差别全在这里**

```text
AlwaysOn                            AsNeeded
┌───────────┐                       ┌───────────┐
│ item1     │                       │ item1     │
│           │                       │           │
│ ▓▓▓▓▓▓▓▓▓ │ ← 滑块拉满整条          │           │ ← 没有滚动条
│ ▓▓▓▓▓▓▓▓▓ │   = 那条灰柱            │           │   视口宽了 8px
│           │                       │           │
└───────────┘                       └───────────┘
```

所以 `maximum() == 0`（内容刚好装满；这个量的含义见 §3.7"补"一节）时，比例正好是 1，滑块被拉满整条轨道 ——
这就是 §3.7 那条 790px 灰柱的形状来源。至于"这个时刻到底该不该画"，
`AlwaysOn` 说该画（于是你看到灰柱），`AsNeeded` 说不该画（Qt 默认行为）。改法见 §3.7。

**QSS 改不了滑块长度**，只能设下限（`min-height: 10px` 防止内容极多时缩成一个点）；
长度只受「内容量 / 窗口高度」影响。

### 3.7 实测案例（2026-11-26）：一条 790px 的灰色竖柱

现象：聊天窗口左栏（会话列表）右侧出现一条从顶到底的灰色竖条，不像滚动条，像装饰边。

逐像素测量截图里那一列（8px 宽）：

| 元素 | 实测 | 尺寸 |
|---|---|---|
| 轨道 | y 61 → 853 | 高 ≈ **793 px** |
| 滑块 `handle`（`#adaba9` = `@scrollbar@`） | y 62 → 851 | 高 ≈ **790 px（99.6%）** |
| `sub-page` 蓝 / `sub-line` 黄 | 顶端 x=106 | 各 **1 × 1 px** |
| `add-line` 绿 / `add-page` 红 | 底端 x=106 | 各 **1 × 1 px** |

结论：那四个子控件**不是"区域小"，而是被铺满的滑块整块盖住了**。
圆角 `border-radius: 4px` 让滑块的四个角漏出底下 1~2 个像素，
于是你在顶端看到"蓝中带黄"、底端"红中带绿"——其实就是被盖住的 line / page 各漏了一行。

**根因在 C++，不在 QSS**：

```cpp
// chatuserlist.cpp:110-118
if (event->type() == QEvent::Enter)
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);    // ← 问题在这
else if (event->type() == QEvent::Leave)
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
```

鼠标一进入列表就**强制显示**滚动条，哪怕列表只有 1 条会话、根本没有可滚动范围
（`maximum() == 0`）。Qt 对"显示了滚动条但范围为空"的渲染方式就是**把滑块拉满整条轨道**，
于是得到一条 790px 的实心灰柱。

**补：`maximum()` 是什么意思**

| 接口 | 含义 | 本项目实测 |
|---|---|---|
| `minimum()` | 最小偏移量 | 恒为 **0** |
| `value()` | 当前滚动偏移量 | 0 ~ `maximum()` |
| `maximum()` | **最大可滚动偏移量 = 内容总高 − 视口高**（单位**像素**，不是条目数） | 1 条会话时 = **0** |
| `pageStep()` | 翻一页走多远（≈ 视口高） | 793 |
| `singleStep()` | 滚一格走多远（滚轮用） | 由 Qt 估算 |

所以 `maximum()` 不是"内容的最高点"，而是"**最多还能往下滚多少**"。
内容比视口还矮时，`setRange(0, 负数)` 会被 Qt 钳成 `maximum = 0` ——
因此 **`maximum() == 0` 就是"没有可滚动范围"的准确判据**，不用自己算高度差。

对照证据：三处"悬停才显示滚动条"的逻辑里，只有 `ContactUserList` 用的是
`Qt::ScrollBarAsNeeded`（`contactuserlist.cpp:97`），那个列表就不会出现这条柱 ——
说明问题出在这两个文件用了 `AlwaysOn`。

**修复**（一行，与 `ContactUserList` 保持一致，**已于 2026-11-26 应用**）：

```cpp
// chatuserlist.cpp:114
- this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
+ this->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
```

```cpp
// searchlist.cpp:43（同一个问题）
- this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
+ this->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
```

`AsNeeded` 的语义是"有范围才显示"，而 `Leave` 时仍然设 `AlwaysOff`，
所以"悬停才出现滚动条"的效果不变，只是**范围为空时不再硬画一条**。

> 经验：看到"很粗的一条滚动条"或"滚动条一直在"，先查两件事 ——
> ① `maximum()` 是不是 0；② 策略是不是被 `AlwaysOn` 强行打开了。

### 3.8 自己验证四个区域

临时加这几条（**排查完要撤**）：

```css
QScrollBar::add-page:vertical  { background-color: red; }
QScrollBar::sub-page:vertical  { background-color: blue; }
QScrollBar::add-line:vertical  { background-color: green; height: 8px; }
QScrollBar::sub-line:vertical  { background-color: yellow; height: 8px; }
```

- 红/蓝会**贴在滑块上下**、随拖动变长变短 → 这就是 page
- 绿/黄**钉在两端不动** → 这就是 line
- 注意 `height: 8px` 必须写进这些临时规则里：QSS 是**按属性**合并的，不是整块覆盖；
  只写 `background-color` 的话，原来的 `height: 0` 仍然生效，8px 永远出不来
- 想证明"被滑块盖住"，把 `::handle` 的 `border-radius` 改成 `0px`：角上那几个色点会消失

---

## 4. `border-image` 和 `background-image` 的区别

| 属性 | 行为 | 用在哪 |
|---|---|---|
| `border-image` | 按九宫格切分后**拉伸铺满**整个控件 | 所有图标按钮（`#add_btn`、`#emo_lb`、`#pwdVisbleLabel`…） |
| `background-image` | 默认按原尺寸绘制，不缩放不平铺 | 需要精确定位的背景图 |

这就是本项目里「按钮配图」一律用 `border-image` 的原因：图片会跟着控件尺寸走，
按钮多大图就多大，不用为每种尺寸准备一张图。

> 注意 `url()` 里的资源路径要带扩展名。整理时发现过一处
> `url(:/res/invisible_hover)` 漏了 `.png`，那条规则静默失效，
> 表现为「注册页确认密码框的显示按钮悬停时图标不见了」。

---

## 5. 四个「写了但不生效」的坑

### 5.1 `::focus` 不存在

```css
/* ✗ QListWidget 没有名为 focus 的子控件，这条永远不匹配 */
#search_list::focus { border: none; }

/* ✓ 想要「获得焦点时」用单冒号伪状态 */
#search_list:focus { border: none; }
```

整理时删掉了 3 条 `::focus`（`#search_list` / `#chat_user_list` / `#con_user_list`），
共 12 行死规则。

### 5.2 给 `QWidget` 子类设背景要重写 `paintEvent`

Qt 的 `QWidget` 子类默认不画 QSS 背景。标准做法：

```cpp
void MyWidget::paintEvent(QPaintEvent* e){
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);   // 让 QSS 生效
    // ... 自己的绘制
}
```

Qt 6 对 `background` / `border` 等少数属性已经能直接生效（`#tool_wid` 就没重写 `paintEvent`），
但行为不完全一致，踩过一次之后建议：**要 QSS 背景的 QWidget 子类，统一加上面这段**。

### 5.3 `QScrollArea` 的 viewport 会自己刷白底

给 `QScrollArea` 的内容控件设了 `background: transparent`，
聊天区仍然是白的 —— 因为**装内容的 viewport 用调色板的 `Base` 角色填了背景**。
代码侧要补：

```cpp
scrollArea->viewport()->setAutoFillBackground(false);
contentWidget->setAutoFillBackground(false);
```

**凡是 `QAbstractScrollArea` 都有这一层**：`QScrollArea` / `QListWidget` / `QTextEdit` 各有一份，
而且 **QSS 管不到**（viewport 没有 objectName）。判断方法：给它设了 `background: transparent`
却还是白/灰，就先怀疑 viewport。

2026-11-27 统一处理了一次，**把这件事挪到各自控件的构造函数里**（自包含，谁都不用记得）：

| 控件 | 位置 |
|---|---|
| `ChatUserList` | `chatuserlist.cpp` 构造函数 |
| `ContactUserList` | `contactuserlist.cpp` 构造函数 |
| `ApplyFriendList` | `applyfriendlist.cpp` 构造函数 |
| `SearchList` | `searchlist.cpp` 构造函数 |
| `ChatView` 里的 `QScrollArea` | `chatview.cpp` 构造函数（顺带 `setFrameShape(NoFrame)`） |
| 申请/验证好友对话框的 `scrollArea` | `applyfriend.cpp` / `authenfriend.cpp` 构造函数（注意这两个 `.ui` 里都叫 `scrollArea`，要改两处） |

> 以前是在 `ChatDialog` 里手动关前两个，结果 `apply_friend_list` 和 `search_list` 被漏掉 ——
> "新的朋友"页整块白底、壁纸透不出来，就是这么来的。
> **新加一个列表时，记得在自己的构造函数里补这一行。**

**配套的 QSS 写法**：`#scrollArea` 只留 `border: none`，**不要**再写 `background-color` ——
那层被 viewport 和内容容器完全覆盖，写了也看不见（真正的 owner 是 `#scrollcontent` / `#chat_bg`）。
同理 `#ApplyFriend` 里那条 `background` 只在对话框边缘露出，也不是内容区的 owner。

### 5.4 objectName 拼错一个字母，规则静默失效

2026-11-26 修掉的一个真实案例：

| 位置 | 名字 |
|---|---|
| `logindialog.ui` | `name="forgetLable"` ← 真实名字（`Lable`，拼错） |
| `logindialog.cpp` | `ui->forgetLable`（3 处，与 .ui 一致） |
| QSS | `#forgetLabel`（手写时"顺手纠正"成 Label）→ **不匹配** |
| QSS | `#forget_label`（又写了个下划线版）→ **幽灵选择器** |

结果：「忘记密码」这个链接的**悬停变蓝从来没生效过**。
normal 态用的是 `@textStrong@`（`#1e1e1e`），和默认黑几乎一样，所以肉眼看不出来；
只有悬停时"该变蓝却没变"暴露了它，而这一点很容易被当成"设计如此"。

**教训：QSS 里的 `#id` 必须从 `.ui` 里复制粘贴，不能凭记忆手打。**
这类错误不报错、不警告，是 QSS 里最难查的一类。

当时项目里 `Lable` 拼写一共 6 处（`pwdLable`、`forgetLable`、`userLable`、
`emailLable`、`varifyCodeLable`），已在同一次改动里全部更正为 `Label`
（`.ui` 6 处 + `logindialog.cpp` 3 处 + QSS 3 个选择器）。

同一天用下面的自查方法还查出**第二条同类死规则**：`#scrollcontent #apply_lb` 里的
`apply_lb` 在 `.ui` 中实际叫 `apple_lb`（`apply` 记成了 `apple`），
导致「申请添加好友」标题的 16px 字体规则从未生效；两个 `.ui` 里的名字已改正。
另删掉一条彻底无主的 `#LineItem` 规则（全项目无此控件）。

> 自查方法：把 QSS 里所有 `#id` 抓出来，和 `.ui` 里所有 `name="..."` 做集合差集，
> 差出来的就是死规则。本项目整理后已无差异（除纯代码创建的控件）。

---

## 6. 中文注释必须用 `fromUtf8` 读

`QFile::readAll()` 返回 `QByteArray`（裸字节，不关心编码），
`QString` 内部是 UTF-16，中间必须显式解码：

```cpp
QString qss = QString::fromUtf8(f.readAll());   // ✓ 中文注释不乱码
QString qss = f.readAll();                      // ✗ 隐式走 fromLatin1，中文全变问号
```

Latin-1 只覆盖 ASCII 和西欧字符，任何中文都会坏掉。
本项目 QSS 里有中文注释，所以 `ThemeManager::buildQss()` 必须用 `fromUtf8`。

---

## 7. 令牌机制（`@token@`）

### 7.1 三层结构

```text
ThemeTokens.h  ──(默认值/字段)──▶  theme.ini（%APPDATA%/llfc/llfcchat/theme.ini）
        │                                   │
        │ buildQss() 读模板                 │ load() 读回
        ▼                                   ▼
stylesheet.tokened.qss  ──替换 @token@──▶  最终的整份 QSS  ──▶ qApp->setStyleSheet()
```

`@token@` 只是占位符，**运行时**被替换成真实色值。
所以 QSS 里不该出现裸色值 —— 出现了就说明它不会跟着主题走。

### 7.2 新增一个令牌，只改 `ThemeTokens.h`

2026-11-26 之前要改 3 处（结构体 / `defaultTokens` / `tokenPairs`）**再加 2 处**
（`ThemeManager::load` / `save` 各 18 行手工枚举）。
现在收敛成一张表 `tokenSlots()`，三步：

1. `struct ThemeTokens` 加字段
2. `defaultTokens()` 给默认值
3. `tokenSlots()` 加一行 `{"字段名", &ThemeTokens::字段名}`

`tokenPairs()`、`load()`、`save()` 都遍历这张表，**不用再动**。
`@token@` 字符串也由 key 自动拼出（`"@" + key + "@"`），所以
**字段名 = ini 键名 = `@token@` 名**，三者天然一致。

### 7.3 向下兼容

旧的 `theme.ini` 里没有新令牌的键 → `load()` 读到空、回落 `defaultTokens()` 的值，
不会报错，只是那几个颜色用默认值。

---

## 8. 本次整理的颜色迁移表

工程里所有裸色值都换成了令牌。迁移对照：

| 原色值 | 出现次数 | 新令牌 | 说明 |
|---|---|---|---|
| `#d3d7d4` | 5 | `@itemSelected@` | 列表项选中 |
| `rgb(206,207,208)` | 4 | `@itemHover@` | 列表项悬停 |
| `#d2d2d2` | 4 | `@btnBgHover@` | 胶囊按钮悬停 |
| `#c6c6c6` | 4 | `@btnBgPress@` | 胶囊按钮按下 |
| `#D3D3D3` / `#BEBEBE` | 各 1 | `@btnBgHover@` / `@btnBgPress@` | 申请条目按钮，原本与上面色值不同，已统一 |
| `rgb(153,153,153)` | 4 | `@textMuted@` | 次要文字 |
| `rgb(140,140,140)` | 2 | `@textMuted@` | 同上，三档灰合一 |
| `#a2a2a2` | — | `@textMuted@` | 令牌原值保持不变（三档灰取中间档） |
| `black` / `rgb(0,0,0)` | 4 | `@textStrong@` | 强调文字（`#1e1e1e`，比纯黑柔和） |
| `green` / `red` | 各 1 | `@success@` / `@danger@` | 校验提示 |
| `#f7f7f8`（边框） | 2 | `@bg@` | 与页面底同色的「假无边框」 |
| `#eaeaea`（分隔线） | 2 | `@borderLight@` | |
| `#ececec` / `#ede9e7` | 3 | `@borderLight@` | 五种浅边框收敛成三档（`border` / `borderLight` / `borderStrong`） |
| `#dbd9d9` | 1 | `@borderStrong@` | 申请条目底部 2px 分隔线 |
| `#fdfdfd` | 1 | `@surface@` | `#grid_wid`，与白色只差 2/255，视为同一个 |
| `black`（背景） | 1 | `transparent` | `#chat_data_list`，聊天条目之间的缝隙 |

**视觉变化**（都是有意为之，验收时对一下）：

1. 会话列表 / 联系人列表的名字：16px → **14px**（原本被后面重复定义的 16px 覆盖，
   属于事故，见 §9）
2. 三档灰文字统一成 `#a2a2a2`，其中原 `#8c8c8c` 的地方略变浅
3. 纯黑文字 → `#1e1e1e`
4. 校验提示的绿/红 → 主题绿 `#07c160` / 主题红 `#fa5151`
5. 注册页「确认密码」的显示按钮，悬停时图标**从无到有**（修好了漏扩展名）

---

## 9. 整理时修掉的 4 个真 bug

| # | 问题 | 后果 |
|---|---|---|
| 1 | `#con_user_list` / `#GroupTipItem` / `#new_friend_item` / `#LineItem` 全套规则被复制了两遍（84 行） | 无视觉后果，纯冗余 |
| 2 | `#user_name_lb` 定义两次（14px / 16px），且是全局 ID 选择器 | 后写的 16px 把会话列表、联系人列表的名字一起顶掉了；改成 `ChatUserWid #user_name_lb` / `ConUserItem #user_name_lb` / `ApplyFriendItem #user_name_lb` 三个作用域 |
| 3 | `url(:/res/invisible_hover)` 漏 `.png` | 注册页显示密码按钮悬停无图标 |
| 4 | 3 条 `::focus` 规则 | 永不生效，12 行死代码 |

---

## 10. 故意保留的「死色」

下面 4 个颜色**没有**令牌化，因为它们在设计上是单点色，不属于主题色系
（令牌化反而会让「换主题时这些也跟着变」变成错误行为）：

| 色值 | 用在哪 | 为什么保留 |
|---|---|---|
| `#d3eaf8` | `#input_tip_wid` 提示条 | 只出现一次，浅蓝提示 |
| `#daf6e7` | `#FriendLabel` 已选标签底 | 只出现一次，浅绿 |
| `#f2eada` | `LoadingDlg` 加载框底 | 只出现一次，暖米色 |
| `rgb(42,112,241)` | `#forgetLabel` 悬停 | 唯一一处「链接蓝」 |

QSS 里这 4 处都写了行尾注释，扫到时不必怀疑是漏改。

---

## 11. 遗留与待办

| 项 | 说明 |
|---|---|
| ~~滚动条"恒显"~~ **已修** | `chatuserlist.cpp:114` 与 `searchlist.cpp:43` 已从 `ScrollBarAlwaysOn` 改为 `ScrollBarAsNeeded`（与 `contactuserlist.cpp:97` 一致）。范围为空时不再画满格滑块，详见 §3.7 |
| `MessageTextEdit` 的透明底 | 仍在 C++ 里写死：`textbubble.cpp` 的 `setStyleSheet("QTextEdit{background:transparent;border:none}")`；等有对应 objectName 再搬进 QSS |
| `QScrollArea` viewport 白底 | 见 §5.3，壁纸模式下聊天区仍不透，需代码侧 `setAutoFillBackground(false)` |
| 按面板分别设透明度（M3.2） | 现在 `wallpaperOverride()` 里是写死的 7 条 rgba 规则，计划改成 `SurfaceSpec` 表驱动 |
| 按面板分别模糊（M3.3） | 现在整张壁纸一个模糊半径；Web 的 `backdrop-filter` 在 Qt 里要自己裁图 + 分级缓存 |
| `hover` 令牌名字 | 现在只用于「分组标题带 / 搜索无结果提示」这两块浅灰底，历史原因沿用 `hover` 这个名字，没改成 `bandBg`（改了会让旧 `theme.ini` 的键失效） |
| **「忘记密码」的开关语义** | `forgetLabel` 用 `ClickedLabel`，点一次永久停在 `selected`，所以 §02 里必须保留 `selected` / `selected_hover` 两条兜底（删掉会导致"点击后悬停不再变蓝"）。它应该是**链接**而非可选中控件：改成点完 `ResetNormalState()` 后可精简为两条，见 [改造记录.md](改造记录.md) **BL-32** |
| **发送者名字随区域变亮变暗**（💤 暂缓） | `#chat_user_name` 现在由 `_toneBody`（壁纸下方 5/6 的**平均值**）统一决定黑/白，覆盖不了"上亮下暗"的壁纸 —— 平均 143 判偏亮，下半部就成了深字压深底。三种方案（A 分档+`[tone=]` 动态属性 / B 衬底或阴影 / C 逐条映射）与取舍见 [改造记录.md](改造记录.md) **BL-33**；2026-11-27 决定暂时搁置 |

---

## 12. 滚动条 overlay：`setLayout(pHLayout_2)` 到底做了什么

`chatview.cpp:38` 有这么一段：

```cpp
//把垂直ScrollBar放到上边 而不是原来的并排
QHBoxLayout *pHLayout_2 = new QHBoxLayout();
pHLayout_2->addWidget(pVScrollBar, 0, Qt::AlignRight);
pHLayout_2->setContentsMargins(0,0,0,0);
_m_pScrollArea->setLayout(pHLayout_2);      // ← 这一行
```

### 12.1 先排除一个常见误判

很多人（包括一开始的本文档）会以为这行"无效"，因为 `QWidget::setLayout()` 对
"已有布局"的控件会警告并 `return`：

```cpp
if (layout()) { qWarning("... which already has a layout"); return; }
```

但那是**普通 QWidget** 的规则。`QAbstractScrollArea`（`QScrollArea` / `QListWidget` /
`QTextEdit` 的基类）**从不给自己装 QLayout** —— 它的 viewport 与滚动条由内部的
`layoutChildren()` 直接摆位置。所以：

```cpp
qDebug() << scrollArea->layout();   // nullptr
scrollArea->setLayout(hlayout);     // 成功，不报警告
```

判断方法就这一行：`layout()` 为 `nullptr` → `setLayout` 会生效；非空 → 会被拒，**且新布局没人回收（泄漏）**。

### 12.2 实测：它确实把滚动条"抬起来"了

探针：400×300 的 `QScrollArea`，默认滚动条宽 14px（未套 QSS）。

| 时刻 | viewport | 滚动条 | 是否重叠 |
|---|---|---|---|
| `setLayout(h)` 之后 | (1,1) **398×298** | (385,1) 14×298 | **是，浮在内容上** |
| 再 `setPolicy(AlwaysOn)` | (1,1) **384×298** | (385,1) 14×298 | 否，变为并排 |

三步原理：

1. `QAbstractScrollArea` 原本把滚动条放在内部容器 `QAbstractScrollAreaScrollBarContainer` 里；
2. `pHLayout_2->addWidget(pVScrollBar, ...)` 把它**从容器里拽出来**，reparent 成滚动区的直接子控件；
3. 此时策略是 `ScrollBarAlwaysOff`，滚动区**不再为它预留宽度** → viewport 占满 → 滚动条浮在内容右缘上。

所以那句注释说的是 **overlay（浮在内容上层）而不是并排（占宽度）** ——
这跟微信那种"细滚动条压在内容上、不挤占宽度"的效果一致，**它是实现成功了的**。

### 12.3 三个风险

| # | 风险 | 说明 |
|---|---|---|
| 1 | **隐含依赖 `AlwaysOff`** | 一旦有人把策略改成 `AlwaysOn` / `AsNeeded`，viewport 立刻缩掉滚动条的宽度（398→384），滚动条从"浮着"变成"并排占位"。这个前提没写在注释里 |
| 2 | **依赖未公开的内部实现** | 把滚动条从内部容器 reparent 出来没有任何文档背书，Qt 改 `layoutChildren()` 的时序或容器结构就可能失效 |
| 3 | **两个几何管理者并存** | `layoutChildren()` 管 viewport、`pHLayout_2` 管滚动条，互不知情；1px 边距、384/398 的差异都是"碰巧"的结果，推理成本高 |

### 12.4 建议

| 方案 | 说明 |
|---|---|
| 保持现状 + 补注释 | 成本最低。至少写明"依赖 `AlwaysOff`；改策略会让滚动条从 overlay 变并排" |
| 显式做 overlay | 不装布局，`bar->setParent(sa)` + `resizeEvent` 里 `bar->setGeometry(...)`；意图清楚，不依赖副作用 |
| `setViewportMargins` | 想让滚动条**占位**而不是浮着时的正规做法 |

### 12.5 顺带：为什么 `#chat_area` 的滚动条看起来"不占宽度"

`ChatView` 用 `setHidden(false)`（而不是改 policy）来在悬停时显示滚动条，
policy 始终是 `AlwaysOff` → viewport 不会被重新缩窄 → 滚动条一直保持浮在内容上。
这条 overlay 效果能成立，靠的正是"永远不改 policy"这个前提。
