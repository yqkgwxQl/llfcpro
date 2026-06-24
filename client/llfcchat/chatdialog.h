#ifndef CHATDIALOG_H
#define CHATDIALOG_H

#include <QDialog>
#include <customizeedit.h>
#include "global.h"

/*.box {
    margin:  20px;   元素外面空 20px —— 推开别的元素
    padding: 15px;   内容向里缩 15px —— 让文字不贴边
    border:  2px solid black;
    } */

/*
clickedbtn和clickedlabel的区别 ——————> 私认为：1.主要是需不需要点击事件，这个的区别 ！！！！！！！！！！！！！！！！！
        2.  clickedlabel ——>  void SetState(QString normal="", QString hover="", QString press="",
                  QString select="", QString select_hover="", QString select_press="");
            clickedBtn ———> void SetState(QString normal,QString hover,QString press);
        clickedBtn 不关心选不选中，clickedLabel需要有选中和不选中的区分
*/
class StateWidget;
namespace Ui {
class ChatDialog;
}

class ChatDialog : public QDialog
{
    Q_OBJECT
    friend class CustomizeEdit;
public:
    explicit ChatDialog(QWidget *parent = nullptr);
    ~ChatDialog();
public slots:
    void slot_load_chat_user();
    void slot_side_chat();
    void slot_side_contact();
    void slot_text_changed(const QString& text);
protected:
    bool eventFilter(QObject *, QEvent *) override;
private:
    /*为什么要这样写，里面有个bool bsearch ，我也太理解 ————> 这个函数的意思是：
        告诉我是否要进入搜索模式（bsearch），如果是（true）就切换到搜索面板；如果不是（false），就恢复到进入搜索之前的面板。
    bsearch = false 时逻辑有个问题了：退出搜索时该显示哪个面板？聊天列表还是联系人列表？ ——> 就要靠 _state 来记录了。*/
    void ShowSearch(bool bsearch);
    void addChatUserList();
    /*  _state        = 进入搜索前你在干什么（历史状态）
        _mode         = 你现在在干什么（当前模式）*/
    ChatUIMode _mode;
    ChatUIMode _state;
    // bool _b_loading;
    QList<StateWidget*> _lb_list;
    void addLBGroup(StateWidget* state_widget);
    void clearLabelState(StateWidget* cur_state_widget);
    Ui::ChatDialog *ui;
};

#endif // CHATDIALOG_H
