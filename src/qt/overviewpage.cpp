// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/overviewpage.h>
#include <qt/forms/ui_overviewpage.h>

#include <qt/bitcoinunits.h>
#include <qt/clientmodel.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/transactionfilterproxy.h>
#include <qt/transactiontablemodel.h>
#include <qt/walletmodel.h>
#include <qt/mainmenupanel.h>

#include <QAbstractItemDelegate>
#include <QPainter>

#define DECORATION_SIZE 54
#define NUM_ITEMS 4

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    explicit TxViewDelegate(const PlatformStyle *_platformStyle, QObject *parent=nullptr):
        QAbstractItemDelegate(parent), unit(BitcoinUnits::BTC),
        platformStyle(_platformStyle)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        //QIcon icon = qvariant_cast<QIcon>(index.data(TransactionTableModel::RawDecorationRole));
        QRect mainRect = option.rect;
        //QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        //int xspace = DECORATION_SIZE + 8;
        //int xspace = 0;
        //int ypad = 0;
        //int halfheight = (mainRect.height() - 2*ypad)/2;
        //QRect amountRect(mainRect.left(), mainRect.top()+ypad, mainRect.width(), halfheight);
        //QRect addressRect(mainRect.left(), mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        //icon = platformStyle->SingleColorIcon(icon);
        //icon.paint(painter, decorationRect);

        int pt = 3;
        int padding = 26;

        QRect dateRect(mainRect.left() + padding, mainRect.top() + pt, 209 - padding, mainRect.height() - pt);
        QRect amountRect(mainRect.width() - 410, mainRect.top() + pt, 410 - padding, mainRect.height() - pt);
        QRect labelRect(dateRect.left() + dateRect.width(), mainRect.top() + pt, mainRect.width() - dateRect.width() - amountRect.width(), mainRect.height() - pt);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        //QString address = index.data(Qt::DisplayRole).toString();
        QString label = index.data(TransactionTableModel::LabelRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(value.canConvert<QBrush>())
        {
            QBrush brush = qvariant_cast<QBrush>(value);
            foreground = brush.color();
        }

        painter->setPen(foreground);
        //QRect boundingRect;
        //painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address, &boundingRect);

        //if (index.data(TransactionTableModel::WatchonlyRole).toBool())
        //{
            //QIcon iconWatchonly = qvariant_cast<QIcon>(index.data(TransactionTableModel::WatchonlyDecorationRole));
            //QRect watchonlyRect(boundingRect.right() + 5, mainRect.top()+ypad+halfheight, 16, halfheight);
            //iconWatchonly.paint(painter, watchonlyRect);
        //}

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true, BitcoinUnits::separatorAlways);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(dateRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));
        painter->drawText(labelRect, Qt::AlignLeft|Qt::AlignVCenter, label);

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        //return QSize(DECORATION_SIZE, DECORATION_SIZE);
        return QSize(27, 27);
    }

    int unit;
    const PlatformStyle *platformStyle;

};
#include <qt/overviewpage.moc>

OverviewPage::OverviewPage(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    clientModel(0),
    walletModel(0),
    currentBalance(-1),
    currentUnconfirmedBalance(-1),
    currentImmatureBalance(-1),
    currentWatchOnlyBalance(-1),
    currentWatchUnconfBalance(-1),
    currentWatchImmatureBalance(-1),
    txdelegate(new TxViewDelegate(platformStyle, this)),
    mainMenu(nullptr)
{
    ui->setupUi(this);

    // use a SingleColorIcon for the "out of sync warning" icon
    //QIcon icon = platformStyle->SingleColorIcon(":/icons/warning");
    //icon.addPixmap(icon.pixmap(QSize(64,64), QIcon::Normal), QIcon::Disabled); // also set the disabled icon because we are using a disabled QPushButton to work around missing HiDPI support of QLabel (https://bugreports.qt.io/browse/QTBUG-42503)
    //ui->labelTransactionsStatus->setIcon(icon);
    //ui->labelWalletStatus->setIcon(icon);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    //ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    //ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    connect(ui->btnSend, SIGNAL(clicked()), this, SLOT(onSendClick()));
    connect(ui->btnTransactions, SIGNAL(clicked()), this, SLOT(onTransactionsClick()));

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
    //connect(ui->labelWalletStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));
    //connect(ui->labelTransactionsStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));
}

void OverviewPage::onSendClick()
{
    if (mainMenu) {
        mainMenu->onSendClick();
    }
}

void OverviewPage::onTransactionsClick()
{
    if (mainMenu) {
        mainMenu->onTransactionsClick();
    }
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        Q_EMIT transactionClicked(filter->mapToSource(index));
}

void OverviewPage::handleOutOfSyncWarningClicks()
{
    Q_EMIT outOfSyncWarningClicked();
}

OverviewPage::~OverviewPage()
{
    delete ui;
}

void OverviewPage::setSyncProgress(double value, double max)
{
    double percent = value * 100.0 / max;
    ui->progressSync->setValue(percent);
    ui->labelProgressSync->setText(QString::number(percent, 'f', 2) + "%");
}

void OverviewPage::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance, const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance)
{
    //int unit = walletModel->getOptionsModel()->getDisplayUnit();
    currentBalance = balance;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    currentWatchOnlyBalance = watchOnlyBalance;
    currentWatchUnconfBalance = watchUnconfBalance;
    currentWatchImmatureBalance = watchImmatureBalance;
    ui->labelBalance->setText(BitcoinUnits::format(BitcoinUnits::Unit::BTC, balance, false, BitcoinUnits::separatorAlways));
    ui->labelUnconfirmed->setText(BitcoinUnits::format(BitcoinUnits::Unit::BTC, unconfirmedBalance, false, BitcoinUnits::separatorAlways));
    //ui->labelImmature->setText(BitcoinUnits::formatWithUnit(unit, immatureBalance, false, BitcoinUnits::separatorAlways));

    //ui->labelTotal->setText(BitcoinUnits::format(unit, balance + unconfirmedBalance + immatureBalance, false, BitcoinUnits::separatorAlways));
    QString textTotal = BitcoinUnits::format(BitcoinUnits::Unit::BTC, balance + unconfirmedBalance, false, BitcoinUnits::separatorAlways);
    int labelTotalMaxWidth = ui->frameTotal->width() - ui->labelTotalCaption->width() - ui->labelTotalUnit->width();
    int fontSize = GUIUtil::getFontPixelSize(textTotal, 5, 28, labelTotalMaxWidth, QString("Roboto Mono"), 700);
    QString totalLabelStyle = "background-color: transparent; color: rgb(255, 198, 0); font-family: \"Roboto Mono\"; font-weight: 700; font-size: ";
    totalLabelStyle = totalLabelStyle + QString(std::to_string(fontSize).c_str()) + QString("px;");
    ui->labelTotal->setStyleSheet(totalLabelStyle);
    ui->labelTotal->setText(textTotal);

    //ui->labelWatchAvailable->setText(BitcoinUnits::formatWithUnit(unit, watchOnlyBalance, false, BitcoinUnits::separatorAlways));
    //ui->labelWatchPending->setText(BitcoinUnits::formatWithUnit(unit, watchUnconfBalance, false, BitcoinUnits::separatorAlways));
    //ui->labelWatchImmature->setText(BitcoinUnits::formatWithUnit(unit, watchImmatureBalance, false, BitcoinUnits::separatorAlways));
    //ui->labelWatchTotal->setText(BitcoinUnits::formatWithUnit(unit, watchOnlyBalance + watchUnconfBalance + watchImmatureBalance, false, BitcoinUnits::separatorAlways));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    //bool showImmature = immatureBalance != 0;
    //bool showWatchOnlyImmature = watchImmatureBalance != 0;

    // for symmetry reasons also show immature label when the watch-only one is shown
    //ui->labelImmature->setVisible(showImmature || showWatchOnlyImmature);
    //ui->labelImmatureText->setVisible(showImmature || showWatchOnlyImmature);
    //ui->labelWatchImmature->setVisible(showWatchOnlyImmature); // show watch-only immature balance

    CAmount totalBalance = balance + unconfirmedBalance;
    if (totalBalance != 0) {
        double availablePercent = balance * 100.0 / totalBalance;
        double pendingPercent = unconfirmedBalance * 100.0 / totalBalance;
        ui->progressAvailable->setValue(availablePercent);
        ui->progressPending->setValue(pendingPercent);
    } else {
        ui->progressAvailable->setValue(0);
        ui->progressPending->setValue(0);
    }
}

// show/hide watch-only labels
void OverviewPage::updateWatchOnlyLabels(bool showWatchOnly)
{
    //ui->labelSpendable->setVisible(showWatchOnly);      // show spendable label (only when watch-only is active)
    //ui->labelWatchonly->setVisible(showWatchOnly);      // show watch-only label
    //ui->lineWatchBalance->setVisible(showWatchOnly);    // show watch-only balance separator line
    //ui->labelWatchAvailable->setVisible(showWatchOnly); // show watch-only available balance
    //ui->labelWatchPending->setVisible(showWatchOnly);   // show watch-only pending balance
    //ui->labelWatchTotal->setVisible(showWatchOnly);     // show watch-only total balance

    //if (!showWatchOnly)
        //ui->labelWatchImmature->hide();
}

void OverviewPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        // Show warning if this is a prerelease version
        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());
    }
}

void OverviewPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter.reset(new TransactionFilterProxy());
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter.get());
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getUnconfirmedBalance(), model->getImmatureBalance(),
                   model->getWatchBalance(), model->getWatchUnconfirmedBalance(), model->getWatchImmatureBalance());
        connect(model, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)), this, SLOT(setBalance(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

        updateWatchOnlyLabels(model->haveWatchOnly());
        connect(model, SIGNAL(notifyWatchonlyChanged(bool)), this, SLOT(updateWatchOnlyLabels(bool)));
    }

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        if(currentBalance != -1)
            setBalance(currentBalance, currentUnconfirmedBalance, currentImmatureBalance,
                       currentWatchOnlyBalance, currentWatchUnconfBalance, currentWatchImmatureBalance);

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString &warnings)
{
    //this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    //this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    //ui->labelWalletStatus->setVisible(fShow);
    //ui->labelTransactionsStatus->setVisible(fShow);
}
