#ifndef SEARCHLIST_H
#define SEARCHLIST_H
#include <QListWidget>
#include "userdata.h"

class CustomizeEdit;
class LoadingDialog;
class UserMgr;
class SearchList: public QListWidget{
    Q_OBJECT
public:
    SearchList(QWidget *parent = nullptr);
    // void setSearchEdit(CustomizeEdit& search_edit);
    void setSearchEdit(CustomizeEdit* search_edit);
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    void addTipItem();
    void CloseFindDlg();
    //c++多态
    // std::shared_ptr<QDialog> _find_dlg;
    QDialog* _find_dlg;

    bool _send_pending;     //表示正在发送中，不允许其它交互（这个发送指的是searchlist.cpp发送搜索请求，从远端服务器获取想要搜索的用户的信息
    void waitPending(bool pending);

    // std::shared_ptr<LoadingDialog> _loading_dlg;
    LoadingDialog* _loading_dlg;

    // std::shared_ptr<CustomizeEdit> _search_edit;
    CustomizeEdit* _search_edit;
public slots:
    void slot_user_search(std::shared_ptr<SearchInfo> si);
    void slot_item_clicked(QListWidgetItem *item);
signals:
    void sig_jump_chat_item_by_search(std::shared_ptr<SearchInfo> si);
};

#endif // SEARCHLIST_H
