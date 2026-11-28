#include "backgroundwid.h"
#include "ThemeManager.h"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsBlurEffect>
#include <QScreen>
#include <QGuiApplication>

BackgroundWid::BackgroundWid(QWidget* parent)
    :QWidget(parent){
    this->setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

//这里其实是专门加载壁纸的，原生的除壁纸之外的内容主要是通过qss进行加载的
void BackgroundWid::refresh(){

    const QString wallpaper_path = ThemeManager::GetInstance()->wallpaperPath();
    const bool pathChanged = (_source_path != wallpaper_path);

    if(wallpaper_path.isEmpty())
        _source = QImage(); //空image
    // //如果源图为空，即没有设置壁纸 **或者** 原图壁纸和wallpaper_path不一样，即更新壁纸
    else if(_source.isNull() || _source_path != wallpaper_path){
        QImage img(wallpaper_path);
        if (!img.isNull()){
            // 大图先缩到屏幕尺寸以内，避免常驻几十 MB
            const QSize screen = QGuiApplication::primaryScreen()->size();
            if (img.width() > screen.width() || img.height() > screen.height())
                img = img.scaled(screen, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            _source = img;
            _source_path = wallpaper_path;
        }
    }

    const WallpaperLayoutMode newMode = ThemeManager::GetInstance()->wallpaperMode();
    //清理缓存
    const QString bg = ThemeManager::GetInstance()->tokens().bg;
    if(pathChanged || _mode != newMode || _cacheBg != bg) {
        _canvaCaches.clear();
        _cacheBg = bg;
    }

    _mode = newMode;

    update();
}

void BackgroundWid::setRegions(const QVector<BackgroundRegion>& regions){
    _regions = regions;
    _canvaCaches.clear();
    update();
}

void BackgroundWid::removeBackgroundRegions(const QVector<BackgroundRegion>& regions){
    for (const auto& region : regions) {
        for (int i = _regions.size() - 1; i >= 0; --i) {
            if (_regions.at(i)._key == region._key) {
                _regions.removeAt(i);
            }
        }
    }
    update();
}

void BackgroundWid::addBackgroundRegions(const QVector<BackgroundRegion>& regions){
    for(const auto& region : regions){
        bool has = false;
        for(const auto& r : _regions){
            if(region._key == r._key)
                has = true;
        }
        if(!has)
            _regions.push_back(region);
    }
    update();
}

void BackgroundWid::updateBackgroundRegions(const QVector<BackgroundRegion>& regions){
    for(const auto& region : regions){
        bool found = false;
        for(auto& r : _regions){
            if(region._key == r._key){
                found = true;
                r._rect = region._rect;
            }
        }
    if(!found)
        _regions.push_back(region);
    }
    update();   //将来在不改变 _bg 尺寸的情况下调用（比如侧栏宽度调整、主题切换后重算区域）
}

void BackgroundWid::resizeEvent(QResizeEvent* e){
    _canvaCaches.clear();
    QWidget::resizeEvent(e);
}

void BackgroundWid::paintEvent(QPaintEvent *event){

    QPainter painter(this);
    if(_source.isNull() || _regions.empty())
        return;

    for(auto& region : _regions){
        const QRect region_rect = region._rect.intersected(rect());  //intersected 相交
        const QColor color = ThemeManager::GetInstance()->surfaceColor(region._key);
        const int realWpAlpha = ThemeManager::GetInstance()->realRegionWpAlpha(region._key);
        const int tintAlpha = ThemeManager::GetInstance()->surfaceTintAlpha(region._key);
        int blur = ThemeManager::GetInstance()->surfaceBlur(region._key);
        if(blur < 0)
            blur = ThemeManager::GetInstance()->wallpaperBlur();

        const QPixmap canva = canvaPixmap(blur);

        painter.save();
        // ① 底色（主题 bg）——壁纸半透明时，透出来的就是它
        painter.fillRect(region_rect, color);
        painter.setOpacity(realWpAlpha / 255.0);
        //② 这里要对图做模糊处理，同时缓存模糊图; 然后将对应的模糊图画到指定region
        painter.drawPixmap(region_rect, canva, region_rect);
        //painter.drawPixmap(rect1, pixmap, rect2) ————> 从 pixmap 的 rect2 区域取出一块图像，然后绘制到当前绘图设备上的 rect1 区域。rect1和rect2大小不同会进行缩放
        painter.setOpacity(1);
        // ③ 遮罩色，按自己的强度
        QColor mask = color;
        mask.setAlpha(tintAlpha);
        painter.fillRect(region_rect, mask);
        painter.restore();
    }

}

QSize BackgroundWid::finalSize(WallpaperLayoutMode mode, const QSize& src, const QSize& target){
    switch(mode){
    case WallpaperLayoutMode::Fill:
        return src.scaled(target, Qt::KeepAspectRatioByExpanding);
    case WallpaperLayoutMode::Fit:
        return src.scaled(target, Qt::KeepAspectRatio);
    case WallpaperLayoutMode::Center:
        return src;
    case WallpaperLayoutMode::Tile:
        return src;
    }
    return src;
}

QPoint BackgroundWid::placement(WallpaperLayoutMode mode, const QSize& bitmap, const QSize& target){
    if(mode == WallpaperLayoutMode::Tile)
        return QPoint(0, 0);    //平铺从左上角开始，其余从中心开始
    else
        return QPoint((target.width() - bitmap.width())/2, (target.height() - bitmap.height())/2);
}

QImage BackgroundWid::burImage(QImage& src, int blur){
    const QRectF srcRect(0, 0, src.width(), src.height());
    QGraphicsScene scene;
    scene.setSceneRect(srcRect);
    auto* item = new QGraphicsPixmapItem(QPixmap::fromImage(src));  // 堆：scene 接管
    auto* fx   = new QGraphicsBlurEffect;                            // 堆：item 接管
    fx->setBlurRadius(blur/8.0);
    item->setGraphicsEffect(fx);
    scene.addItem(item);

    /*
        1.A R G B 32        → 4 个通道，每个 8 位，共 32 位（每像素 4 字节）
        Premultiplied     → RGB 通道"预乘"过 alpha
            预乘的意思：存进去的 RGB 值已经乘过 alpha。

        2.你想画的颜色                      普通 ARGB32 存什么                   Premultiplied 存什么
        半透明红                    (255,0,0, α=128)	(255, 0, 0, 128)	(128, 0, 0, 128) ← RGB 也乘了 128/255

        3.QImage::Format_ARGB32_Premultiplied ————> QPainter画图的最优格式
    */
    QImage out(src.size(), QImage::Format_ARGB32_Premultiplied);
        //out为透明图，作为painter的图底（自己作为图底透明，不要影响在它上面绘制的模糊图）
    out.fill(Qt::transparent);
    QPainter painter(&out);
    scene.render(&painter, QRectF(out.rect()), srcRect);
    painter.end();

    return out;
}
/*
一、先搞懂 render() 的三个参数
    scene.render(&painter, target, source);
    //                    ↑目标矩形  ↑源矩形
    参数              含义              坐标系
    painter         画到哪             由 painter 决定（这里是 out 这张 400×300 的图）
    target          画到画布的哪一块        图像坐标（0,0 是 out 左上角）
    source      取场景的哪一块         场景坐标（item 摆放的坐标系）

    它的工作方式可以总结为一句话：
        把场景中 source 这块区域，缩放/平移后画到画布的 target 这块区域。
    所以：
        source 比 target 大  → 缩小
        source 比 target 小  → 放大
        两者一样大           → 1:1（我们要的）

    默认值是什么
        scene.render(&painter);   // 等价于：
        // target = painter 的整个视口（也就是 out 的 400×300）
        // source = scene.sceneRect()（整个场景的可见范围）
    你只传了 painter，另外两个都是默认值。 问题就出在这。
二、sceneRect 为什么会被模糊“撑大”
    QGraphicsScene 有个属性叫 sceneRect，表示“这个场景的有效范围”。默认值 = 所有 item 的包围盒。
    item 的包围盒本来是 (0, 0, 400, 300)，和你给的图一样大。

    但一旦挂上 QGraphicsBlurEffect，模糊会向四周扩散——为了容纳这些扩散出去的半透明像素，item 的“有效包围盒”会向外扩 radius 那么多：
        没有模糊：  (0, 0, 400, 300)
        模糊半径 10：(-10, -10, 420, 320)
        模糊半径 40：(-40, -40, 480, 380)
        而 sceneRect 跟着变成了这个更大的矩形。
三、把数字走一遍（这是关键）
    假设：图 400×300，模糊半径 10，out 也是 400×300。
    source = sceneRect() = (-10, -10, 420, 320)
    target = out 整块   = (0, 0, 400, 300)
    render 会做：
        横向缩放 = 400 / 420 = 0.952
        纵向缩放 = 300 / 320 = 0.9375
        注意这两个数不一样 → 除了缩小，还会轻微变形。

    再算位置：场景点 (x, y) 映射到画布：
    X' = (x - (-10)) * 0.952 = (x + 10) * 0.952

    于是：
    场景里的点                   画到 out 的哪里
    图片左上角 (0, 0)            (9.5, 9.4) ← 往里缩了约 10 像素
    图片右下角 (400, 300)        (390.5, 290.6) ← 也往里缩了
    结果：图片内容只占 381×281，四周空出约 10 像素的透明边，整体还略微变形。
四、画成图更容易理解
    没有模糊时（1:1，正确）=====================
    sceneRect = (0,0,400,300)              out = 400×300
    ┌──────────────────────┐          ┌──────────────────────┐
    │                      │          │                      │
    │      图片内容         │  ───▶    │      图片内容         │   1:1
    │                      │          │                      │
    └──────────────────────┘          └──────────────────────┘

    有模糊时（sceneRect 被撑大，缩小）=====================
    sceneRect = (-10,-10,420,320)          out = 400×300
    ┌────────────────────────┐        ┌────────────────────┐
    │ ░░░░░░░░░░░░░░░░░░░░░░ │        │░░░░░░░░░░░░░░░░░░░░│
    │ ░┌──────────────────┐░ │        │░┌────────────────┐░│
    │ ░│                  │░ │  ───▶  │░│                │░│  420×320
    │ ░│    图片内容       │░ │        │░│   图片内容      │░│  ↓
    │ ░│                  │░ │        │░└────────────────┘░│  400×300
    │ ░└──────────────────┘░ │        │░░░░░░░░░░░░░░░░░░░░│
    │ ░░░░░░░░░░░░░░░░░░░░░░ │        └────────────────────┘
    └────────────────────────┘
       ░ = 模糊扩散出来的半透明边缘     内容变小 + 四周多出透明边
    ░ 那块是模糊“溢出来”的部分，本来我们应该把它裁掉，但默认行为是把它也一起画进图里，于是压缩了真正的图片内容。
五、修法：把 source 和 target 都固定成“图片原始区域”
    ~~~
        const QRectF srcRect(0, 0, src.width(), src.height());

        QGraphicsScene scene;
        scene.setSceneRect(srcRect);                        // ① 场景范围固定成图片大小

        ...
        scene.render(&painter, QRectF(out.rect()), srcRect); // ② 源和目标都显式给
    ~~~
    现在：
        source = (0,0,400,300)
        target = (0,0,400,300)
        → 缩放 = 1，偏移 = 0，严格 1:1
        模糊扩散到 x<0 或 y>300 的部分会被裁掉——这正是我们想要的：我们只要图片那一块，边缘因为模糊变淡是正常现象（壁纸边缘本来也不需要“溢出”）。
*/


QPixmap BackgroundWid::canvaPixmap(int blur){
    auto it = _canvaCaches.find(blur);
    if (it != _canvaCaches.end() && it.value().size() == size())
        return it.value();

    const QSize target_size = finalSize(_mode, _source.size(), size());
    const QPoint place = placement(_mode, target_size, size()); //不同的mode可能有不同的size，所以size()和target_size是不一样的，eg:Fill和Fit可能会比size()大、小

    QImage canva(size(), QImage::Format_ARGB32_Premultiplied);
    canva.fill(QColor(ThemeManager::GetInstance()->tokens().bg));

    QPainter canvas_painter(&canva);
    const QImage scaled_image = _source.scaled(target_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (_mode == WallpaperLayoutMode::Tile)
        canvas_painter.drawTiledPixmap(rect(), QPixmap::fromImage(scaled_image));
    else
        canvas_painter.drawImage(place, scaled_image);          // pt 可能为负（Fill/Center 超出控件）
    canvas_painter.end();

    QImage out = canva;
    if (blur > 0) {
        // 先缩到 1/8 再模糊，最后放大 —— 和 renderBitmap 同一套思路
        const QSize small(qMax(8, size().width()/8), qMax(8, size().height()/8));
        QImage tiny = canva.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        tiny = burImage(tiny, blur);
        out = tiny.scaled(size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    QPixmap pm = QPixmap::fromImage(out);
    _canvaCaches[blur] = pm;
    return pm;
}
/*
二、三个角色：QImage / QPixmap / QPainter
    类               是什么                                             类比
    QImage	CPU 里的像素数组，可读写、可缩放、可做算法（模糊、取平均色）        一张"数据表"
    QPixmap	为显示优化的位图，可能存在显存里，画得快但不好改                一张"贴纸"
    QPainter	画笔，负责把 QPixmap/QImage/矩形/文字 画到某个"画布"上       一支笔

    分工很典型：
    读文件 → QImage（要缩放、要模糊，都是 CPU 运算）
           → 做完之后 QPixmap::fromImage()（转成贴纸，方便快速绘制）
           → QPainter::drawPixmap() 贴到控件上

    为什么不是一开始就用 QPixmap？因为 QPixmap 做缩放/模糊这类逐像素操作很慢，而且某些平台不允许直接访问像素。
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
四种模式，一句话 + 一张图

① Fill（填充 / cover）——铺满，多的裁掉
 y = -50   ← 图片顶边（在控件外，被裁掉）
     ┌────────────────────────┐
     │                        │
     │                        │  y = 0    ← 控件上边
     │   ┌────────────────┐   │
     │   │                │   │
     │   │   能看见的部分   │   │
     │   │                │   │
     │   └────────────────┘   │
     │                        │  y = 300  ← 控件下边
     │                        │
     └────────────────────────┘
  y = 350   ← 图片底边（在控件外，被裁掉）

② Fit（适应 / contain）——完整显示，留边
图片缩放成 300×300（完整塞进 400×300），左右各留 50

 ┌────────────────────────────┐
 │        ┌──────────┐        │
 │        │          │        │
 │  留边  │ 300×300  │  留边  │
 │        │          │        │
 │        └──────────┘        │
 └────────────────────────────┘
结果矩形 QRect(50, 0, 300, 300) —— 图一定完整，但四周可能露出底色。

③ Center（居中）——不缩放，原尺寸放中间
图片仍是 600×600，比控件大，居中后四周都超出

      ┌────────────────────────┐  ← y = -150
 ┌────┼────────────────────────┼────┐ ← y = 0
 │    │                        │    │
 │    │     中间能看见的部分      │    │
 │    │                        │    │
 └────┼────────────────────────┼────┘ ← y = 300
      └────────────────────────┘  ← y = 450
结果矩形 QRect(-100, -150, 600, 600) —— 图比控件大就被裁，比控件小就露底。

④ Tile（平铺）——不缩放，重复铺
图片仍是 600×600，但按"贴瓷砖"的方式从左到右、从上到下重复

 ┌────────────────────────────┐
 │  ┌───┬───┬───┬───┬───┐     │
 │  │   │   │   │   │   │     │
 │  ├───┼───┼───┼───┼───┤     │
 │  │   │   │   │   │   │     │
 │  └───┴───┴───┴───┴───┘     │
 └────────────────────────────┘
它不靠 targetRect，而是调 p.drawTiledPixmap(rect(), pm) —— 所以 targetRect 里 case Tile 直接返回整块 (0,0,400,300)，具体平铺交给绘制函数。
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
三、QPainter 必备的 8 个调用
    QPainter p(this);                 // ① 构造：把 this（当前控件）当画布
    p.fillRect(rect(), QColor(...));  // ② 填满矩形（不透明覆盖）
    p.setOpacity(0.6);                // ③ 之后所有绘制都按 60% 透明
    p.drawPixmap(targetRect, pm);     // ④ 把 pm 缩放画进 targetRect
    p.drawPixmap(QPoint(x,y), pm);    // ⑤ 原尺寸，左上角对齐 (x,y)
    p.drawTiledPixmap(rect(), pm);    // ⑥ 平铺（小图铺满整块）
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);  // ⑦ 缩放时平滑
    p.save(); p.restore();            // ⑧ 保存/恢复画笔状态（opacity、clip、transform）
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

缩放：QImage::scaled() 的三种模式（M3 最容易搞混）
    ```
        QImage::scaled(size, aspectMode, transformMode)
        aspectMode                      行为                      用在哪
        IgnoreAspectRatio           强行拉成目标尺寸，会变形        不建议用于壁纸
        KeepAspectRatio             保持比例完整                  放进目标框，留边	"适应/contain"模式
        KeepAspectRatioByExpanding	保持比例                     铺满目标框，超出裁掉	"填充/cover"模式（M3 默认）
    ```
举个具体例子，帮你建立数字感：

    控件 400×300，图片 1000×1000
    KeepAspectRatio          → 缩成 300×300，两边各留 50 的空
    KeepAspectRatioByExpanding → 缩成 400×400，上下各超出 50
    transformMode 用 Qt::FastTransformation（快、有锯齿）或 Qt::SmoothTransformation（慢一点、平滑）。壁纸用 Smooth。

*/