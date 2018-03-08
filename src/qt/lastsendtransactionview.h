#ifndef LASTSENDTRANSACTIONVIEW_H
#define LASTSENDTRANSACTIONVIEW_H

#include <QWidget>
#include <qt/guiutil.h>

class PlatformStyle;
class TransactionStatus;

namespace Ui {
class LastSendTransactionView;
}

class LastSendTransactionView : public QWidget
{
    Q_OBJECT

public:
    explicit LastSendTransactionView(const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~LastSendTransactionView();

    void setAmount(const CAmount& balance);
    void setStatus(TransactionStatus& status);
    void setTime(qint64 time);
    void setAdress(const std::string& adress);
private:
    void setDone();
    void setPending();
    void setFailed();
private:
    Ui::LastSendTransactionView *ui;
};

#endif // LASTSENDTRANSACTIONVIEW_H
