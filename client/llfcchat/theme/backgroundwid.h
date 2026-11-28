#ifndef BACKGROUNDWID_H
#define BACKGROUNDWID_H

#include <QWidget>
#include "global.h"

class BackgroundWid: public QWidget
{
    Q_OBJECT
public:
    struct BackgroundRegion{
        QString _key;
        QRect _rect;
    };
public:
    explicit BackgroundWid(QWidget* parent);

    void refresh();         // 从 ThemeManager  重新读一遍配置
    void setRegions(const QVector<BackgroundRegion>& regions);
    void removeBackgroundRegions(const QVector<BackgroundRegion>& regions);
    void addBackgroundRegions(const QVector<BackgroundRegion>& regions);
    void updateBackgroundRegions(const QVector<BackgroundRegion>& regions);

protected:
    virtual void resizeEvent(QResizeEvent* e) override;
    virtual void paintEvent(QPaintEvent* event)override;
private:
    QSize finalSize(WallpaperLayoutMode mode, const QSize& src, const QSize& target);    //图最终占多少像素
    QPoint placement(WallpaperLayoutMode mode, const QSize& bitmap, const QSize& target); //左上角放在哪里
    QImage burImage(QImage& src, int blur);

    QImage _source = QImage();  //已缩到不超过屏幕尺寸的原图
    QString _source_path = "";
    WallpaperLayoutMode _mode = WallpaperLayoutMode::Fill;


    QVector<BackgroundRegion> _regions;
    QHash<int, QPixmap> _canvaCaches;
    QString _cacheBg;          // 上次建画布时用的 tokens().bg
    //把“整张壁纸按当前模式铺满整个控件”预先画成一张离屏图（canvas），供各个区域裁剪。 ————> 返回canva_pixmap的同时，缓存canva_pixmap
        //即返回的是一张和控件一样大的图
    QPixmap canvaPixmap(int blur);
};

#endif // BACKGROUNDWID_H
