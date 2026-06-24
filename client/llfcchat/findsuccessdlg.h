#ifndef FINDSUCCESSDLG_H
#define FINDSUCCESSDLG_H

#include <QDialog>
#include "userdata.h"

namespace Ui {
class FindSuccessDlg;
}

class FindSuccessDlg : public QDialog
{
    Q_OBJECT

public:
    explicit FindSuccessDlg(QWidget *parent = nullptr);
    ~FindSuccessDlg();
    void setSearchInfo(std::shared_ptr<SearchInfo> search_info);
private:
    Ui::FindSuccessDlg *ui;
    QWidget * _parent;
    std::shared_ptr<SearchInfo> _si;
public slots:
    void on_add_friend_btn_clicked();
};

#endif // FINDSUCCESSDLG_H
