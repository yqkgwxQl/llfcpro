#include "chatdialog.h"
#include "ui_chatdialog.h"
#include <QRandomGenerator>
#include <chatuserwid.h>
#include <QMovie>
#include <QTimer>
#include "statewidget.h"
#include <QMouseEvent>

// #include "customizeedit.h"

ChatDialog::ChatDialog(QWidget *parent)
    : QDialog(parent), _mode(ChatUIMode::ChatMode),_state(ChatUIMode::ChatMode),ui(new Ui::ChatDialog){
    ui->setupUi(this);
    ui->add_btn->SetState("normal","hover","press");
    ui->search_edit->SetMaxLength(15);
    ui->search_edit->setPlaceholderText(QStringLiteral("搜索"));  //QStringLiteral : 编译时就生成好 QString 所需的内部数据，运行时零开销
    ShowSearch(false);
    //连接加载信号和槽
    connect(ui->chat_user_list,&ChatUserList::sig_loading_chat_user,this,&ChatDialog::slot_load_chat_user);
    addChatUserList();

    QPixmap pixmap(":/res/head_1.jpg");
    QPixmap scale_pixmap = pixmap.scaled(ui->side_head_lb->size(),Qt::KeepAspectRatio);
    ui->side_head_lb->setPixmap(scale_pixmap);
    ui->side_head_lb->setScaledContents(true); // 告诉 QLabel，以后不管 label 尺寸怎么变，图片自动跟着拉伸填满。
    ui->side_chat_lb->setState("normal","hover","pressed","selected_normal","selected_hover","selected_pressed");
    ui->side_contact_lb->setState("normal","hover","pressed","selected_normal","selected_hover","selected_pressed");

    addLBGroup(ui->side_chat_lb);
    addLBGroup(ui->side_contact_lb);
    connect(ui->side_chat_lb, &StateWidget::clicked, this, &ChatDialog::slot_side_chat);
    connect(ui->side_contact_lb, &StateWidget::clicked, this, &ChatDialog::slot_side_contact);

    connect(ui->search_edit,&QLineEdit::textChanged,this,&ChatDialog::slot_text_changed);
    // connect(ui->search_edit, &CustomizeEdit::sig_clear_search, this, [this](){
    //     ShowSearch(false);
    // });
    this->installEventFilter(this);
    ui->side_chat_lb->setSelected(true);
}

ChatDialog::~ChatDialog(){
    delete ui;
}

void ChatDialog::slot_load_chat_user(){
    // if(!_b_loading){
    //     return;
    // }
    // _b_loading = true;
    // LoadingDialog* loading_dlg = new LoadingDialog(this);
    // loading_dlg->setModal(true);
    // loading_dlg->show();
    QLabel *loading_item = new QLabel(this);
    QMovie *movie = new QMovie(":/res/loading.gif");

    loading_item->setMovie(movie);
    loading_item->setFixedSize(250, 70);
    loading_item->setAlignment(Qt::AlignCenter);
    movie->setScaledSize(QSize(50, 50));

    QListWidgetItem *item = new QListWidgetItem;
    item->setSizeHint(QSize(250, 70));
    ui->chat_user_list->addItem(item);
    ui->chat_user_list->setItemWidget(item, loading_item);
    movie->start();
    qDebug() << "add new data to list.....";
    QApplication::processEvents();
    // QTimer::singleShot(1000, this, [this, item]() {
    //     qDebug() << "add new Data to list...";
    //     addChatUserList();
    //     ui->chat_user_list->takeItem(ui->chat_user_list->row(item));
    //     ui->chat_user_list->update();
    // });
    qDebug() << "add new Data to list...";
    addChatUserList();
    ui->chat_user_list->takeItem(ui->chat_user_list->row(item));
    ui->chat_user_list->update();
    //addChatUserList 本身很快 loading_dlg绘制完，立即销毁，根本看不到
    // addChatUserList();
    // loading_dlg->deleteLater();
    // _b_loading = false;
}

void ChatDialog::slot_side_chat(){
    qDebug()<< "receive side chat clicked";
    clearLabelState(ui->side_chat_lb);
    ui->stackedWidget->setCurrentWidget(ui->chat_page);
    _state = ChatUIMode::ChatMode;
    ShowSearch(false);
}

void ChatDialog::slot_side_contact(){
    qDebug()<< "receive side contact clicked";
    clearLabelState(ui->side_contact_lb);
    ui->stackedWidget->setCurrentWidget(ui->friend_apply_page);
    _state = ChatUIMode::ContactMode;
    ShowSearch(false);
}

void ChatDialog::slot_text_changed(const QString &text){
    if(!text.isEmpty()){
        ShowSearch(true);
    }
}

bool ChatDialog::eventFilter(QObject *watched, QEvent *event){
    if(event->type() == QEvent::MouseButtonPress){
        QMouseEvent* mouse_event = dynamic_cast<QMouseEvent*>(event);
        if(_mode == ChatUIMode::SearchMode){
            // 将鼠标点击位置转换为搜索列表坐标系中的位置
            QPoint pos_in_searchlist = ui->search_list->mapFromGlobal(mouse_event->globalPos());
            // 判断点击位置是否在聊天列表的范围内
            if(!ui->search_list->rect().contains(pos_in_searchlist)){
                // 如果不在聊天列表内，清空输入框
                ui->search_edit->clear();
                ShowSearch(false);
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}

void ChatDialog::ShowSearch(bool bsearch){
    if(bsearch){
        ui->chat_user_list->hide();
        ui->con_user_list->hide();
        ui->search_list->show();
        _mode = ChatUIMode::SearchMode;
    }else{
        if(_state == ChatUIMode::ChatMode){
            ui->search_list->hide();
            ui->con_user_list->hide();
            ui->chat_user_list->show();
        }else if(_state == ChatUIMode::ContactMode){
            ui->search_list->hide();
            ui->chat_user_list->hide();
            ui->con_user_list->show();
        }
    }
}

//-----------------用于测试chat_user_wid----------------------------


void ChatDialog::addChatUserList(){
    // 创建QListWidgetItem，并设置自定义的widget
    for(int i = 0; i < 13; i++){
        int randomValue = QRandomGenerator::global()->bounded(100); // 生成0到99之间的随机整数
        int str_i = randomValue%strs.size();
        int head_i = randomValue%heads.size();
        int name_i = randomValue%names.size();

        auto *chat_user_wid = new ChatUserWid();
        chat_user_wid->SetInfo(names[name_i], heads[head_i], strs[str_i]);
        QListWidgetItem *item = new QListWidgetItem;
        //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
        item->setSizeHint(chat_user_wid->sizeHint());
        ui->chat_user_list->addItem(item);
        ui->chat_user_list->setItemWidget(item, chat_user_wid);
    }
}

void ChatDialog::clearLabelState(StateWidget *cur_state_widget){
    for(auto& state_widget:_lb_list){
        if(state_widget == cur_state_widget)
            continue;
        state_widget->clearState();
    }
}

void ChatDialog::addLBGroup(StateWidget * state_widget){
    _lb_list.push_back(state_widget);
}
//-----------------用于测试chat_user_wid----------------------------