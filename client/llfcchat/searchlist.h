#ifndef SEARCHLIST_H
#define SEARCHLIST_H
#include <QListWidget>
#include "userdata.h"

class SearchList: public QListWidget{
public:
    SearchList(QWidget *parent = nullptr);
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    void addTipItem();
    void CloseFindDlg();
    //c++多态
    std::shared_ptr<QDialog> _find_dlg;
public slots:
    void slot_user_search(std::shared_ptr<SearchInfo>);
    void slot_item_clicked(QListWidgetItem *item);
};

#endif // SEARCHLIST_H
