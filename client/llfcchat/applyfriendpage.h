#ifndef APPLYFRIENDPAGE_H
#define APPLYFRIENDPAGE_H

#include <QWidget>
#include "userdata.h"
#include "applyfrienditem.h"

namespace Ui {
class ApplyFriendPage;
}

class ApplyFriendPage : public QWidget
{
    Q_OBJECT

public:
    explicit ApplyFriendPage(QWidget *parent = nullptr);
    ~ApplyFriendPage();
    void AddNewApply(std::shared_ptr<AddFriendApply> apply);
protected:
    void paintEvent(QPaintEvent *event);
private:
    void loadApplyList();
    Ui::ApplyFriendPage *ui;
    std::unordered_map<int, ApplyFriendItem*> _unauth_items;    //这里我认为根本不需要这个 ×××× ————> 按钮的显示与不显示需要这个

    void loadMoreApply();
    void fflushPage();
    int _fail_count;       //if服务器并未成功返回applyList, 我需要进行重试，if重试了还是失败了，失败3次就停止
    qint64 _min_apply_id;  // 0 = 还没加载过(首页)
    bool _loading_more;     //防重入
    bool _has_more;
public slots:
    void slot_auth_rsp(std::shared_ptr<AuthRsp> );
    void slot_applylist_rsp(const QJsonObject& obj);
signals:
    void sig_show_search(bool);
};

#endif // APPLYFRIENDPAGE_H
