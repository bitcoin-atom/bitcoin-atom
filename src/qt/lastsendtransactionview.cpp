#include <qt/lastsendtransactionview.h>
#include <qt/forms/ui_lastsendtransactionview.h>
#include <qt/lastsendtransactionview.moc>
#include <qt/platformstyle.h>
#include <qt/bitcoinunits.h>
#include <qt/transactionrecord.h>

LastSendTransactionView::LastSendTransactionView(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LastSendTransactionView)
{
    ui->setupUi(this);
}

LastSendTransactionView::~LastSendTransactionView()
{
    delete ui;
}

void LastSendTransactionView::setAmount(const CAmount& balance)
{
    CAmount amount = balance;
    if (amount < 0) {
        amount = -amount;
    }
    ui->amountLabel->setText(BitcoinUnits::format(BitcoinUnits::Unit::BTC_rounded, amount, false, BitcoinUnits::separatorAlways));
}

void LastSendTransactionView::setStatus(TransactionStatus& status)
{
    if (status.status == TransactionStatus::Confirmed) {
        setDone();
    } else if (status.status == TransactionStatus::Conflicted || status.status == TransactionStatus::NotAccepted) {
        setFailed();
    } else {
        setPending();
    }
}

void LastSendTransactionView::setDone()
{
    ui->statusBg->setStyleSheet("background-color: #4f9600;");
    ui->statusIco->setStyleSheet("border-image: url(:/icons/done_ico);");
}

void LastSendTransactionView::setPending()
{
    ui->statusBg->setStyleSheet("background-color: #b2b2c0;");
    ui->statusIco->setStyleSheet("border-image: url(:/icons/pending_ico);");
}

void LastSendTransactionView::setFailed()
{
    ui->statusBg->setStyleSheet("background-color: #c53f4f;");
    ui->statusIco->setStyleSheet("border-image: url(:/icons/error_ico);");
}

void LastSendTransactionView::setTime(qint64 time)
{
    ui->labelWhen->setText(GUIUtil::dateTimeStr(time));
}

void LastSendTransactionView::setAdress(const std::string& adress)
{
    ui->labelTo->setText(adress.c_str());
}
