#include "authenfriend.h"
#include "ui_authenfriend.h"
#include "global.h"
#include <QEvent>
#include <QScrollBar>
#include "usermgr.h"
#include <QFontMetrics>
#include <QJsonObject>
#include "tcpmgr.h"


AuthenFriend::AuthenFriend(QWidget *parent)
    : QDialog(parent), ui(new Ui::AuthenFriend),_label_point(2,6){
    ui->setupUi(this);
    // 隐藏对话框标题栏
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    this->setObjectName("ApplyFriend");
    this->setModal(true);
    ui->name_ed->setPlaceholderText(tr("恋恋风辰"));
    ui->lb_ed->setPlaceholderText("搜索、添加标签");
    ui->back_ed->setPlaceholderText("燃烧的胸毛");

    ui->lb_ed->SetMaxLength(21);
    ui->lb_ed->move(2, 2);
    ui->lb_ed->setFixedHeight(20);
    ui->lb_ed->setMaxLength(10);
    /*                      单位              上限值         对中文的效果
        SetMaxLength(21)	UTF-8 字节        21 字节       约 7 个汉字
        setMaxLength(10)	Unicode 字符      10 字符       10 个汉字*/
    ui->input_tip_wid->hide();
    _tip_cur_point = QPoint(5, 5);

    _tip_data = { "同学","家人","菜鸟教程","C++ Primer","Rust 程序设计",
                 "父与子学Python","nodejs开发指南","go 语言开发指南",
                 "游戏伙伴","金融投资","微信读书","拼多多拼友" };
    connect(ui->more_lb, &ClickedOnceLabel::clicked, this, &AuthenFriend::ShowMoreLabel);
    InitTipLbs();
    //-----------no write---------------------------
    //链接输入标签回车事件
    connect(ui->lb_ed, &CustomizeEdit::returnPressed, this, &AuthenFriend::SlotLabelEnter);
    connect(ui->lb_ed, &CustomizeEdit::textChanged, this, &AuthenFriend::SlotLabelTextChange);
    connect(ui->lb_ed, &CustomizeEdit::editingFinished, this, &AuthenFriend::SlotLabelEditFinished);
    connect(ui->tip_lb, &ClickedOnceLabel::clicked, this, &AuthenFriend::SlotAddFirendLabelByClickTip);

    ui->scrollArea->horizontalScrollBar()->setHidden(true);
    ui->scrollArea->verticalScrollBar()->setHidden(true);
    ui->scrollArea->installEventFilter(this);
    ui->sure_btn->SetState("normal","hover","press");
    ui->cancel_btn->SetState("normal","hover","press");
    //连接确认和取消按钮的槽函数
    connect(ui->cancel_btn, &QPushButton::clicked, this, &AuthenFriend::SlotApplyCancel);
    connect(ui->sure_btn, &QPushButton::clicked, this, &AuthenFriend::SlotApplySure);
    //-----------no write---------------------------
}

AuthenFriend::~AuthenFriend(){
    delete ui;
}

bool AuthenFriend::eventFilter(QObject *obj, QEvent *event){
    if (obj == ui->scrollArea && event->type() == QEvent::Enter)
    {
        ui->scrollArea->verticalScrollBar()->setHidden(false);
    }
    else if (obj == ui->scrollArea && event->type() == QEvent::Leave)
    {
        ui->scrollArea->verticalScrollBar()->setHidden(true);
    }
    return QObject::eventFilter(obj, event);
}

//------------no write-------------------
void AuthenFriend::resetLabels(){
    auto max_width = ui->grid_wid->width();
    auto label_height = 0;
    for(auto iter = _friend_labels.begin(); iter != _friend_labels.end(); iter++){
        if( _label_point.x() + iter.value()->width() > max_width) {
            _label_point.setY(_label_point.y()+iter.value()->height()+6);
            _label_point.setX(2);
        }

        iter.value()->move(_label_point);
        iter.value()->show();

        _label_point.setX(_label_point.x()+iter.value()->width()+2);
        _label_point.setY(_label_point.y());
        label_height = iter.value()->height();
    }

    if(_friend_labels.isEmpty()){
        ui->lb_ed->move(_label_point);
        return;
    }

    if(_label_point.x() + MIN_APPLY_LABEL_ED_LEN > ui->grid_wid->width()){
        ui->lb_ed->move(2,_label_point.y()+label_height+6);
    }else{
        ui->lb_ed->move(_label_point);
    }
}

void AuthenFriend::ShowMoreLabel()
{
    qDebug()<< "receive more label clicked";
    ui->more_lb_wid->hide();

    ui->lb_list->setFixedWidth(325);
    _tip_cur_point = QPoint(5, 5);
    auto next_point = _tip_cur_point;
    std::size_t textWidth;
    std::size_t textHeight;
    //重拍现有的label
    for(auto & added_key : _add_label_keys){
        auto added_lb = _add_labels[added_key];

        QFontMetrics fontMetrics(added_lb->font()); // 获取QLabel控件的字体信息
        textWidth = fontMetrics.horizontalAdvance(added_lb->text()); // 获取文本的宽度
        textHeight = fontMetrics.height(); // 获取文本的高度

        if(_tip_cur_point.x() +textWidth + tip_offset > ui->lb_list->width()){
            _tip_cur_point.setX(tip_offset);
            _tip_cur_point.setY(_tip_cur_point.y()+textHeight+15);
        }
        added_lb->move(_tip_cur_point);

        next_point.setX(added_lb->pos().x() + textWidth + 15);
        next_point.setY(_tip_cur_point.y());

        _tip_cur_point = next_point;

    }

    //添加未添加的
    for(int i = 0; i < _tip_data.size(); i++){
        auto iter = _add_labels.find(_tip_data[i]);
        if(iter != _add_labels.end()){
            continue;
        }

        auto* lb = new ClickedLabel(ui->lb_list);
        lb->SetState("normal", "hover", "pressed", "selected_normal",
                     "selected_hover", "selected_pressed");
        lb->setObjectName("tipslb");
        lb->setText(_tip_data[i]);
        connect(lb, &ClickedLabel::clicked, this, &AuthenFriend::SlotChangeFriendLabelByTip);

        QFontMetrics fontMetrics(lb->font()); // 获取QLabel控件的字体信息
        std::size_t textWidth = fontMetrics.horizontalAdvance(lb->text()); // 获取文本的宽度
        std::size_t textHeight = fontMetrics.height(); // 获取文本的高度

        if (_tip_cur_point.x() + textWidth + tip_offset > ui->lb_list->width()) {

            _tip_cur_point.setX(tip_offset);
            _tip_cur_point.setY(_tip_cur_point.y() + textHeight + 15);

        }

        next_point = _tip_cur_point;

        AddTipLbs(lb, _tip_cur_point, next_point, textWidth);

        _tip_cur_point = next_point;

    }
    //-------调整容器高度
    // 计算展示区需要增加多少高度
    int diff_height = next_point.y() + textHeight + tip_offset - ui->lb_list->height();

    // 设置展示区新高度
    ui->lb_list->setFixedHeight(next_point.y() + textHeight + tip_offset);

    // 同步增高滚动内容区域
    ui->scrollcontent->setFixedHeight(ui->scrollcontent->height() + diff_height);
}

void AuthenFriend::SlotLabelEnter()
{
    if(ui->lb_ed->text().isEmpty()){
        return;
    }

    auto text = ui->lb_ed->text();
    addLabel(ui->lb_ed->text());

    ui->input_tip_wid->hide();
    auto find_it = std::find(_tip_data.begin(), _tip_data.end(), text);
    //找到了就只需设置状态为选中即可
    if (find_it == _tip_data.end()) {
        _tip_data.push_back(text);
    }

    //判断标签展示栏是否有该标签
    auto find_add = _add_labels.find(text);
    if (find_add != _add_labels.end()) {
        find_add.value()->SetCurState(ClickLbState::Selected);
        return;
    }

    //标签展示栏也增加一个标签, 并设置绿色选中
    auto* lb = new ClickedLabel(ui->lb_list);
    lb->SetState("normal", "hover", "pressed", "selected_normal",
                 "selected_hover", "selected_pressed");
    lb->setObjectName("tipslb");
    lb->setText(text);
    connect(lb, &ClickedLabel::clicked, this, &AuthenFriend::SlotChangeFriendLabelByTip);
    qDebug() << "ui->lb_list->width() is " << ui->lb_list->width();
    qDebug() << "_tip_cur_point.x() is " << _tip_cur_point.x();

    QFontMetrics fontMetrics(lb->font()); // 获取QLabel控件的字体信息
    int textWidth = fontMetrics.horizontalAdvance(lb->text()); // 获取文本的宽度
    int textHeight = fontMetrics.height(); // 获取文本的高度
    qDebug() << "textWidth is " << textWidth;

    if (_tip_cur_point.x() + textWidth + tip_offset + 3 > ui->lb_list->width()) {

        _tip_cur_point.setX(5);
        _tip_cur_point.setY(_tip_cur_point.y() + textHeight + 15);

    }

    auto next_point = _tip_cur_point;

    AddTipLbs(lb, _tip_cur_point, next_point, textWidth);
    _tip_cur_point = next_point;

    int diff_height = next_point.y() + textHeight + tip_offset - ui->lb_list->height();
    ui->lb_list->setFixedHeight(next_point.y() + textHeight + tip_offset);

    lb->SetCurState(ClickLbState::Selected);

    ui->scrollcontent->setFixedHeight(ui->scrollcontent->height() + diff_height);
}

void AuthenFriend::SlotLabelTextChange(const QString& text)
{
    if (text.isEmpty()) {
        ui->tip_lb->setText("");
        ui->input_tip_wid->hide();
        return;
    }

    auto iter = std::find(_tip_data.begin(), _tip_data.end(), text);
    if (iter == _tip_data.end()) {
        auto new_text = add_prefix + text;
        ui->tip_lb->setText(new_text);
        ui->input_tip_wid->show();
        return;
    }
    ui->tip_lb->setText(text);
    ui->input_tip_wid->show();
}

void AuthenFriend::SlotLabelEditFinished()
{
    ui->input_tip_wid->hide();
}

void AuthenFriend::SlotAddFirendLabelByClickTip(QString text)
{
    int index = text.indexOf(add_prefix);
    if (index != -1) {
        text = text.mid(index + add_prefix.length());
    }
    addLabel(text);

    auto find_it = std::find(_tip_data.begin(), _tip_data.end(), text);
    //找到了就只需设置状态为选中即可
    if (find_it == _tip_data.end()) {
        _tip_data.push_back(text);
    }

    //判断标签展示栏是否有该标签
    auto find_add = _add_labels.find(text);
    if (find_add != _add_labels.end()) {
        find_add.value()->SetCurState(ClickLbState::Selected);
        return;
    }

    //标签展示栏也增加一个标签, 并设置绿色选中
    auto* lb = new ClickedLabel(ui->lb_list);
    lb->SetState("normal", "hover", "pressed", "selected_normal",
                 "selected_hover", "selected_pressed");
    lb->setObjectName("tipslb");
    lb->setText(text);
    connect(lb, &ClickedLabel::clicked, this, &AuthenFriend::SlotChangeFriendLabelByTip);
    qDebug() << "ui->lb_list->width() is " << ui->lb_list->width();
    qDebug() << "_tip_cur_point.x() is " << _tip_cur_point.x();

    QFontMetrics fontMetrics(lb->font()); // 获取QLabel控件的字体信息
    int textWidth = fontMetrics.horizontalAdvance(lb->text()); // 获取文本的宽度
    int textHeight = fontMetrics.height(); // 获取文本的高度
    qDebug() << "textWidth is " << textWidth;

    if (_tip_cur_point.x() + textWidth+ tip_offset+3 > ui->lb_list->width()) {

        _tip_cur_point.setX(5);
        _tip_cur_point.setY(_tip_cur_point.y() + textHeight + 15);

    }

    auto next_point = _tip_cur_point;

    AddTipLbs(lb, _tip_cur_point, next_point, textWidth);
    _tip_cur_point = next_point;

    int diff_height = next_point.y() + textHeight + tip_offset - ui->lb_list->height();
    ui->lb_list->setFixedHeight(next_point.y() + textHeight + tip_offset);

    lb->SetCurState(ClickLbState::Selected);

    ui->scrollcontent->setFixedHeight(ui->scrollcontent->height()+ diff_height );
}

void AuthenFriend::SlotApplyCancel()
{
    qDebug() << "Slot Apply Cancel";
    this->hide();
    deleteLater();
}

void AuthenFriend::SlotApplySure()
{
    qDebug()<<"Slot Apply Sure called" ;
    //发送请求逻辑
    QJsonObject jsonObj;
    auto uid = UserMgr::GetInstance()->GetUid();
    jsonObj["uid"] = uid;       // 被申请者的uid，即我自己的uid
    auto name = ui->name_ed->text();
    if(name.isEmpty()){
        name = ui->name_ed->placeholderText();
    }

    jsonObj["desc"] = name;  //我对我自己的描述，eg:"你好，我是暗影岛的夏水流萤"

    auto bakname = ui->back_ed->text();
    if(bakname.isEmpty()){
        bakname = ui->back_ed->placeholderText();
    }

    //？？？？？？？？？？？？？？？？？？？？？其实我觉得都不用传bakname,这个bakname只在我本地客户端有用，给远端别的ChatServer看有用吗？没用。
    jsonObj["bakname"] = bakname;  //我给对方起的备注名
    // jsonObj["touid"] = _si->_uid;   //申请者的id，即申请谁向我申请了，我要向他返回认证信息
    jsonObj["touid"] = _apply_info->_uid;

    QJsonDocument doc(jsonObj);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

    //发送tcp请求给chat server
    emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_AUTH_FRIEND_REQ, jsonData);
    this->hide();
    deleteLater();
}
//------------no write-------------------

void AuthenFriend::addLabel(QString name){
    auto find_iter = _friend_labels.find(name);
    if(find_iter != _friend_labels.end()){
        return;
    }
    //完成新建标签的初始化
    auto tmp_label = new FriendLabel(ui->grid_wid);
    tmp_label->SetText(name);
    tmp_label->setObjectName("FriendLabel");
    //计算一下是否超出一行
    auto max_width = ui->grid_wid->width();
    if (_label_point.x() + tmp_label->width() > max_width) {
        _label_point.setY(_label_point.y() + tmp_label->height() + friend_lb_vertical_gap);
        _label_point.setX(2);
    }
    //进行真正的移动同时更新下一个label的位置
    tmp_label->move(_label_point);
    _friend_labels.insert(tmp_label->Text(),tmp_label);
    _friend_label_keys.push_back(tmp_label->Text());
    connect(tmp_label, &FriendLabel::sig_close, this, &AuthenFriend::SlotRemoveFriendLabel);
    _label_point.setX(_label_point.x() + tmp_label->width() + friend_lb_gap);
    _label_point.setY(_label_point.y());
    //这里如果lb_ed 长度不够，要挪到下一行 ; 长度够了，要正常挪到_label_point的位置
    if(_label_point.x() + MIN_APPLY_LABEL_ED_LEN > ui->grid_wid->width()){
        ui->lb_ed->move(2,_label_point.y() + tmp_label->height() + 2);
    }else{
        ui->lb_ed->move(_label_point);
    }
    ui->lb_ed->clear();
    //换行了grid_wid也能显示所有的标签
    if (ui->grid_wid->height() < _label_point.y() + tmp_label->height() + 2) {
        ui->grid_wid->setFixedHeight(_label_point.y() + tmp_label->height() * 2 + 2);
    }
}

void AuthenFriend::SlotRemoveFriendLabel(QString name){
    qDebug() << "receive close signal";
    _label_point.setX(2);
    _label_point.setY(6);

    //删除编辑区标签
    auto find_iter = _friend_labels.find(name);
    if(find_iter == _friend_labels.end())
        return;
    delete find_iter.value();
    _friend_labels.erase(find_iter);

    auto find_key = _friend_label_keys.end();
    for(auto iter = _friend_label_keys.begin(); iter != _friend_label_keys.end(); iter++){
        if(*iter == name){
            find_key = iter;
            break;
        }
    }
    if(find_key != _friend_label_keys.end()){
        _friend_label_keys.erase(find_key);
    }
    //删除编辑区的标签后，也要把展示区对应的标签状态改回"未选中"。
    // ① 重排编辑区剩余标签的位置
    resetLabels();

    // ② 在展示区标签 Map 中查找刚刚删除的标签名
    auto find_add = _add_labels.find(name);
    if(find_add == _add_labels.end()){
        return;  // 展示区没有这个标签？不处理
    }

    // ③ 将展示区对应的 ClickedLabel 恢复为 Normal 状态
    find_add.value()->ResetNormalState();
}

//点击标已有签添加或删除新联系人的标签
void AuthenFriend::SlotChangeFriendLabelByTip(QString lbtext, ClickLbState state){
    auto find_iter = _add_labels.find(lbtext);
    if(find_iter == _add_labels.end()){
        return;
    }
    //对于选中状态的，一定要加到标签栏里
    if(state == ClickLbState::Selected){
        //编写添加逻辑
        addLabel(lbtext);
        return;
    }
    //对于没选中，但点击的一定要移除(其实已经点击两次了，第一次加入到标签栏里，第二次从标签栏里移除)
    if(state == ClickLbState::Normal){
        //编写删除逻辑
        SlotRemoveFriendLabel(lbtext);
        return;
    }
}

//因为此时还未与服务器联调数据，此时我们写一个InitLabel的函数模拟创建多个标签展示

//后期搜索用户功能用户数据会从服务器传回来，所以写了下面的接口
void AuthenFriend::SetSearchInfo(std::shared_ptr<SearchInfo> si){
    _si = si;
    auto applyname = UserMgr::GetInstance()->GetName();
    auto bakname = si->_name;
    ui->name_ed->setText(applyname);
    ui->back_ed->setText(bakname);
}

// void AuthenFriend::SetSearchInfo(std::shared_ptr<ApplyInfo> apply_info){
//     _si = std::make_shared<SearchInfo>();
//     auto applyname = UserMgr::GetInstance()->GetName();
//     auto bakname = si->_name;
//     ui->name_ed->setText(applyname);
//     ui->back_ed->setText(bakname);
// }

void AuthenFriend::InitTipLbs(){
    int lines = 1;
    for(int i = 0; i < _tip_data.size(); i++){
        auto* lb = new ClickedLabel(ui->lb_list);
        lb->SetState("normal", "hover", "pressed", "selected_normal","selected_hover", "selected_pressed");
        lb->setObjectName("tipslb");
        lb->setText(_tip_data[i]);
        /*说实话，我还是没理解为什么用text_width不用label->width()？？？？？？？？？？？？？？？？？？？？？？/
         * label->width() 是 label 控件当前实际的大小，但刚创建时还没被设过大小，它只是默认值。
         * 而 fontMetrics.width(text) 是文本内容应该需要的大小，计算精确、不受控件状态影响。排版判断需要"应该多大"而不是"当前多大"。*/
        connect(lb, &ClickedLabel::clicked, this, &AuthenFriend::SlotChangeFriendLabelByTip);
        QFontMetrics font_metrics(lb->font());
        std::size_t text_width = font_metrics.horizontalAdvance(lb->text());
        std::size_t text_height = font_metrics.height();
        /*那为什么不像std::size_t text_height = font_metrics.height();一样，font_metrics.horizontalAdvance()
            也就是为什么算height不需要加上参数，算width需要加上参数？
        height() 不需要参数对于同一个字体和字号，所有文本的高度都是一样的。*/
        //为什么不用text_width + tip_gap + _tip_cur_point.x()  这里 tip_offset = 5 表示：文本离容器右边至少留 5px 的呼吸空间，不至于紧贴着边。
        if(text_width + tip_offset + _tip_cur_point.x() > ui->lb_list->width()){
            lines++;
            if(lines > 2){
                delete lb;
                return;
            }
            _tip_cur_point.setX(tip_offset);    // X = 5（回到最左边）
            _tip_cur_point.setY(_tip_cur_point.y() + text_height + 15); // Y 向下移
        }
        auto next_point = _tip_cur_point;
        AddTipLbs(lb,_tip_cur_point,next_point,text_width);
        _tip_cur_point = next_point;
    }
}

void AuthenFriend::AddTipLbs(ClickedLabel* lb,QPoint cur_point,QPoint& next_point,std::size_t text_width){
    lb->move(cur_point);
    lb->show();
    _add_labels.insert(lb->text(),lb);
    _add_label_keys.push_back(lb->text());
    // next_point.setX(lb->pos().x() + text_width + tip_gap);
    // next_point.setY(lb->pos().y());
    next_point.setX(cur_point.x() + text_width + tip_gap);
    next_point.setY(cur_point.y());
}

void AuthenFriend::SetApplyInfo(std::shared_ptr<ApplyInfo> apply_info){
    _apply_info = apply_info;
    ui->back_ed->setPlaceholderText(apply_info->_name);
}