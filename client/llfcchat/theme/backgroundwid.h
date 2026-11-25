#ifndef BACKGROUNDWID_H
#define BACKGROUNDWID_H

#include <QWidget>
#include "global.h"

class BackgroundWid: public QWidget
{
    Q_OBJECT
public:

    explicit BackgroundWid(QWidget* parent);

    void SetImagePath(const QString& path); // 传空 = 清除
    void SetOpacity(int percent);           // 10~100
    void SetBlurRadius(int redius);         // 0~40
    void SetLayoutMode(WallpaperLayoutMode mode);
    void refresh();         // 从 ThemeManager  重新读一遍配置

protected:
    virtual void paintEvent(QPaintEvent* event)override;
private:
    QSize finalSize(WallpaperLayoutMode mode, const QSize& src, const QSize& target);    //图最终占多少像素
    QPoint placement(WallpaperLayoutMode mode, const QSize& bitmap, const QSize& target); //左上角放在哪里

    QImage burImage(QImage& src, int blur);
    QImage renderBitmap(const QImage& src, const QSize& fs, int blur);  //渲染要绘制的那张位图（缩放 + 模糊 + 不透明）
    QPixmap cachedBitmap(const QSize& fs);

    //是否需要重算模糊
    bool _blurDirty = false;
    QImage _source = QImage();  //已缩到不超过屏幕尺寸的原图
    QString _source_path = "";
    int _blur = 0;
    WallpaperLayoutMode _mode = WallpaperLayoutMode::Fill;
    int _opacity = 100;
    int _scrimAlpha = 0;

    //由于模糊处理会消耗大量资源, therefore这里对模糊原图、模糊图尺寸、模糊半径进行缓存
    QPixmap _cache = QPixmap();
    QSize _cacheFs = QSize(-1, -1);
    int _cacheBlur = 0;
    WallpaperLayoutMode _cacheMode = WallpaperLayoutMode::Fill;
};

#endif // BACKGROUNDWID_H
