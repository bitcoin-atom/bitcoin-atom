// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/sendcoinsdialog.h>
#include <qt/forms/ui_sendcoinsdialog.h>

#include <qt/addresstablemodel.h>
#include <qt/bitcoinunits.h>
#include <qt/clientmodel.h>
#include <qt/coincontroldialog.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/sendcoinsentry.h>
#include <qt/transactionrecord.h>

#include <base58.h>
#include <chainparams.h>
#include <wallet/coincontrol.h>
#include <validation.h> // mempool and minRelayTxFee
#include <ui_interface.h>
#include <txmempool.h>
#include <policy/fees.h>
#include <wallet/fees.h>

#include <qt/lastsendtransactionview.h>
#include <qt/changefeedialog.h>
#include <qt/forms/ui_changefeedialog.h>

#include <qt/stockinfo.h>
#include <qt/pricewidget.h>

#include <QFontMetrics>
#include <QScrollBar>
#include <QSettings>
#include <QTextDocument>
#include <QSpacerItem>

#define TARGETS_COUNT 9

static const std::array<int, TARGETS_COUNT> confTargets = { {2, 4, 6, 12, 24, 48, 144, 504, 1008} };
int getConfTargetForIndex(int index) {
    if (index+1 > static_cast<int>(confTargets.size())) {
        return confTargets.back();
    }
    if (index < 0) {
        return confTargets[0];
    }
    return confTargets[index];
}
int getIndexForConfTarget(int target) {
    for (unsigned int i = 0; i < confTargets.size(); i++) {
        if (confTargets[i] >= target) {
            return i;
        }
    }
    return confTargets.size() - 1;
}

SendCoinsDialog::SendCoinsDialog(const PlatformStyle *_platformStyle, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SendCoinsDialog),
    changeFeeDialog(nullptr),
    clientModel(0),
    model(0),
    fNewRecipientAllowed(true),
    fFeeMinimized(true),
    platformStyle(_platformStyle),
    view1(nullptr),
    view2(nullptr),
    view3(nullptr),
    spacer1(nullptr)
{
    ui->setupUi(this);

    changeFeeDialog = new ChangeFeeDialog(this);

    if (!_platformStyle->getImagesOnButtons()) {
        //ui->addButton->setIcon(QIcon());
        //ui->clearButton->setIcon(QIcon());
        //ui->sendButton->setIcon(QIcon());
    } else {
        //ui->addButton->setIcon(_platformStyle->SingleColorIcon(":/icons/add"));
        //ui->clearButton->setIcon(_platformStyle->SingleColorIcon(":/icons/remove"));
        //ui->sendButton->setIcon(_platformStyle->SingleColorIcon(":/icons/send"));
    }

    //GUIUtil::setupAddressWidget(ui->lineEditCoinControlChange, this);

    addEntry();

    //connect(ui->addButton, SIGNAL(clicked()), this, SLOT(addEntry()));
    //connect(ui->clearButton, SIGNAL(clicked()), this, SLOT(clear()));

    // Coin Control
    //connect(ui->pushButtonCoinControl, SIGNAL(clicked()), this, SLOT(coinControlButtonClicked()));
    //connect(ui->checkBoxCoinControlChange, SIGNAL(stateChanged(int)), this, SLOT(coinControlChangeChecked(int)));
    //connect(ui->lineEditCoinControlChange, SIGNAL(textEdited(const QString &)), this, SLOT(coinControlChangeEdited(const QString &)));

    // Coin Control: clipboard actions
    QAction *clipboardQuantityAction = new QAction(tr("Copy quantity"), this);
    QAction *clipboardAmountAction = new QAction(tr("Copy amount"), this);
    QAction *clipboardFeeAction = new QAction(tr("Copy fee"), this);
    QAction *clipboardAfterFeeAction = new QAction(tr("Copy after fee"), this);
    QAction *clipboardBytesAction = new QAction(tr("Copy bytes"), this);
    QAction *clipboardLowOutputAction = new QAction(tr("Copy dust"), this);
    QAction *clipboardChangeAction = new QAction(tr("Copy change"), this);
    connect(clipboardQuantityAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardQuantity()));
    connect(clipboardAmountAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardAmount()));
    connect(clipboardFeeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardFee()));
    connect(clipboardAfterFeeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardAfterFee()));
    connect(clipboardBytesAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardBytes()));
    connect(clipboardLowOutputAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardLowOutput()));
    connect(clipboardChangeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardChange()));
    //ui->labelCoinControlQuantity->addAction(clipboardQuantityAction);
    //ui->labelCoinControlAmount->addAction(clipboardAmountAction);
    //ui->labelCoinControlFee->addAction(clipboardFeeAction);
    //ui->labelCoinControlAfterFee->addAction(clipboardAfterFeeAction);
    //ui->labelCoinControlBytes->addAction(clipboardBytesAction);
    //ui->labelCoinControlLowOutput->addAction(clipboardLowOutputAction);
    //ui->labelCoinControlChange->addAction(clipboardChangeAction);

    connect(ui->btnChangeFee, SIGNAL(clicked()), this, SLOT(onChangeClick()));

    // init transaction fee section
    QSettings settings;
    if (!settings.contains("fFeeSectionMinimized"))
        settings.setValue("fFeeSectionMinimized", true);
    if (!settings.contains("nFeeRadio") && settings.contains("nTransactionFee") && settings.value("nTransactionFee").toLongLong() > 0) // compatibility
        settings.setValue("nFeeRadio", 1); // custom
    if (!settings.contains("nFeeRadio"))
        settings.setValue("nFeeRadio", 0); // recommended
    if (!settings.contains("nSmartFeeSliderPosition"))
        settings.setValue("nSmartFeeSliderPosition", 0);
    if (!settings.contains("nTransactionFee"))
        settings.setValue("nTransactionFee", (qint64)DEFAULT_TRANSACTION_FEE);
    if (!settings.contains("fPayOnlyMinFee"))
        settings.setValue("fPayOnlyMinFee", false);
    if (!settings.contains("nCustomRadio")) {
        settings.setValue("nCustomRadio", 0);
    }
    changeFeeDialog->ui->groupFee->setId(changeFeeDialog->ui->radioSmartFee, 0);
    changeFeeDialog->ui->groupFee->setId(changeFeeDialog->ui->radioCustomFee, 1);
    changeFeeDialog->ui->groupFee->button((int)std::max(0, std::min(1, settings.value("nFeeRadio").toInt())))->setChecked(true);
    changeFeeDialog->ui->customFee->setValue(settings.value("nTransactionFee").toLongLong());

    changeFeeDialog->ui->groupCustom->setId(changeFeeDialog->ui->checkBoxCustomFee, 0);
    changeFeeDialog->ui->groupCustom->setId(changeFeeDialog->ui->checkBoxMinimumFee, 1);
    changeFeeDialog->ui->groupCustom->button((int)std::max(0, std::min(1, settings.value("nCustomRadio").toInt())))->setChecked(true);

    minimizeFeeSection(settings.value("fFeeSectionMinimized").toBool());

    spacer1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    view1 = new LastSendTransactionView(platformStyle, this);
    view2 = new LastSendTransactionView(platformStyle, this);
    view3 = new LastSendTransactionView(platformStyle, this);
    ui->lastTrLayout->addWidget(view1);
    ui->lastTrLayout->addWidget(view2);
    ui->lastTrLayout->addWidget(view3);
    ui->lastTrLayout->addItem(spacer1);
}

void SendCoinsDialog::addPriceWidget(StockInfo* stockInfo)
{
    PriceWidget *priceWidget = new PriceWidget(stockInfo, this);
    ui->priceLayout->addWidget(priceWidget);
    QSpacerItem *spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->priceLayout->addItem(spacer);
}

void SendCoinsDialog::onChangeClick()
{
    changeFeeDialog->exec();
}

void SendCoinsDialog::setClientModel(ClientModel *_clientModel)
{
    this->clientModel = _clientModel;

    if (_clientModel) {
        connect(_clientModel, SIGNAL(numBlocksChanged(int,QDateTime,double,bool)), this, SLOT(updateSmartFeeLabel()));
    }
}

void SendCoinsDialog::setModel(WalletModel *_model)
{
    this->model = _model;

    if(_model && _model->getOptionsModel())
    {
        for(int i = 0; i < ui->entries->count(); ++i)
        {
            SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
            if(entry)
            {
                entry->setModel(_model);
            }
        }

        setBalance(_model->getBalance(), _model->getUnconfirmedBalance(), _model->getImmatureBalance(),
                   _model->getWatchBalance(), _model->getWatchUnconfirmedBalance(), _model->getWatchImmatureBalance());
        connect(_model, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)), this, SLOT(setBalance(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)));
        connect(_model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
        updateDisplayUnit();

        // Coin Control
        connect(_model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(coinControlUpdateLabels()));
        connect(_model->getOptionsModel(), SIGNAL(coinControlFeaturesChanged(bool)), this, SLOT(coinControlFeatureChanged(bool)));
        //ui->frameCoinControl->setVisible(_model->getOptionsModel()->getCoinControlFeatures());
        coinControlUpdateLabels();

        // fee section
        //for (const int &n : confTargets) {
            //ui->confTargetSelector->addItem(tr("%1 (%2 blocks)").arg(GUIUtil::formatNiceTimeOffset(n*Params().GetConsensus().nPowTargetSpacing)).arg(n));
        //}
        changeFeeDialog->ui->confTargetSelector->setMinimum(0);
        changeFeeDialog->ui->confTargetSelector->setMaximum(TARGETS_COUNT - 1);
        connect(changeFeeDialog->ui->confTargetSelector, SIGNAL(valueChanged(int)), this, SLOT(updateSmartFeeLabel()));
        connect(changeFeeDialog->ui->confTargetSelector, SIGNAL(valueChanged(int)), this, SLOT(coinControlUpdateLabels()));

        connect(changeFeeDialog->ui->groupFee, SIGNAL(buttonClicked(int)), this, SLOT(updateFeeSectionControls()));
        connect(changeFeeDialog->ui->groupFee, SIGNAL(buttonClicked(int)), this, SLOT(coinControlUpdateLabels()));

        connect(changeFeeDialog->ui->customFee, SIGNAL(valueChanged()), this, SLOT(coinControlUpdateLabels()));

        connect(changeFeeDialog->ui->groupCustom, SIGNAL(buttonClicked(int)), this, SLOT(setMinimumFee()));
        connect(changeFeeDialog->ui->groupCustom, SIGNAL(buttonClicked(int)), this, SLOT(updateFeeSectionControls()));
        connect(changeFeeDialog->ui->groupCustom, SIGNAL(buttonClicked(int)), this, SLOT(coinControlUpdateLabels()));

        connect(changeFeeDialog->ui->optInRBF, SIGNAL(stateChanged(int)), this, SLOT(updateSmartFeeLabel()));
        connect(changeFeeDialog->ui->optInRBF, SIGNAL(stateChanged(int)), this, SLOT(coinControlUpdateLabels()));
        changeFeeDialog->ui->customFee->setSingleStep(GetRequiredFee(1000));
        updateFeeSectionControls();
        updateMinFeeLabel();
        updateSmartFeeLabel();

        // set default rbf checkbox state
        changeFeeDialog->ui->optInRBF->setCheckState(Qt::Checked);

        // set the smartfee-sliders default value (wallets default conf.target or last stored value)
        QSettings settings;
        if (settings.value("nSmartFeeSliderPosition").toInt() != 0) {
            // migrate nSmartFeeSliderPosition to nConfTarget
            // nConfTarget is available since 0.15 (replaced nSmartFeeSliderPosition)
            int nConfirmTarget = 25 - settings.value("nSmartFeeSliderPosition").toInt(); // 25 == old slider range
            settings.setValue("nConfTarget", nConfirmTarget);
            settings.remove("nSmartFeeSliderPosition");
        }
        if (settings.value("nConfTarget").toInt() == 0)
            changeFeeDialog->ui->confTargetSelector->setValue(getIndexForConfTarget(model->getDefaultConfirmTarget()));
        else
            changeFeeDialog->ui->confTargetSelector->setValue(getIndexForConfTarget(settings.value("nConfTarget").toInt()));

        updateLastTransactions();
        connect(_model, SIGNAL(onCashedTransactionUpdate()), this, SLOT(onCashedTransactionUpdate()));
    }
}

void SendCoinsDialog::updateLastTransactions()
{
    TransactionRecord tr1;
    TransactionRecord tr2;
    TransactionRecord tr3;
    int count = model->getLastTransactions(tr1, tr2, tr3);
    if (count == 1) {
        view1->setVisible(true);
        view2->setVisible(false);
        view3->setVisible(false);
    } else if (count == 2) {
        view1->setVisible(true);
        view2->setVisible(true);
        view3->setVisible(false);
    } else if (count == 3) {
        view1->setVisible(true);
        view2->setVisible(true);
        view3->setVisible(true);
    } else {
        view1->setVisible(false);
        view2->setVisible(false);
        view3->setVisible(false);
    }
    view1->setAmount(tr1.debit + tr1.credit);
    view1->setStatus(tr1.status);
    view1->setTime(tr1.time);
    view1->setAdress(tr1.address);

    view2->setAmount(tr2.debit + tr2.credit);
    view2->setStatus(tr2.status);
    view2->setTime(tr2.time);
    view2->setAdress(tr2.address);

    view3->setAmount(tr3.debit + tr3.credit);
    view3->setStatus(tr3.status);
    view3->setTime(tr3.time);
    view3->setAdress(tr3.address);
}

void SendCoinsDialog::onCashedTransactionUpdate()
{
    updateLastTransactions();
}

SendCoinsDialog::~SendCoinsDialog()
{
    QSettings settings;
    settings.setValue("fFeeSectionMinimized", fFeeMinimized);
    settings.setValue("nFeeRadio", changeFeeDialog->ui->groupFee->checkedId());
    settings.setValue("nCustomRadio", changeFeeDialog->ui->groupCustom->checkedId());
    settings.setValue("nConfTarget", getConfTargetForIndex(changeFeeDialog->ui->confTargetSelector->value()));
    settings.setValue("nTransactionFee", (qint64)changeFeeDialog->ui->customFee->value());
    //settings.setValue("fPayOnlyMinFee", changeFeeDialog->ui->checkBoxMinimumFee->isChecked());

    delete changeFeeDialog;
    delete ui;
}

void SendCoinsDialog::on_sendButton_clicked()
{
    if(!model || !model->getOptionsModel())
        return;

    QList<SendCoinsRecipient> recipients;
    bool valid = true;

    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry)
        {
            if(entry->validate())
            {
                recipients.append(entry->getValue());
            }
            else
            {
                valid = false;
            }
        }
    }

    if(!valid || recipients.isEmpty())
    {
        return;
    }

    fNewRecipientAllowed = false;
    WalletModel::UnlockContext ctx(model->requestUnlock());
    if(!ctx.isValid())
    {
        // Unlock wallet was cancelled
        fNewRecipientAllowed = true;
        return;
    }

    // prepare transaction for getting txFee earlier
    WalletModelTransaction currentTransaction(recipients);
    WalletModel::SendCoinsReturn prepareStatus;

    // Always use a CCoinControl instance, use the CoinControlDialog instance if CoinControl has been enabled
    CCoinControl ctrl;
    if (model->getOptionsModel()->getCoinControlFeatures())
        ctrl = *CoinControlDialog::coinControl();

    updateCoinControlState(ctrl);

    prepareStatus = model->prepareTransaction(currentTransaction, ctrl, true);

    // process prepareStatus and on error generate message shown to user
    processSendCoinsReturn(prepareStatus,
        BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), currentTransaction.getTransactionFee()));

    if(prepareStatus.status != WalletModel::OK) {
        fNewRecipientAllowed = true;
        return;
    }

    CAmount txFee = currentTransaction.getTransactionFee();

    // Format confirmation message
    QStringList formatted;
    for (const SendCoinsRecipient &rcp : currentTransaction.getRecipients())
    {
        // generate bold amount string
        QString amount = "<b>" + BitcoinUnits::formatHtmlWithUnit(model->getOptionsModel()->getDisplayUnit(), rcp.amount);
        amount.append("</b>");
        // generate monospace address string
        QString address = "<span style='font-family: monospace;'>" + rcp.address;
        address.append("</span>");

        QString recipientElement;

        if (!rcp.paymentRequest.IsInitialized()) // normal payment
        {
            if(rcp.label.length() > 0) // label with address
            {
                recipientElement = tr("%1 to %2").arg(amount, GUIUtil::HtmlEscape(rcp.label));
                recipientElement.append(QString(" (%1)").arg(address));
            }
            else // just address
            {
                recipientElement = tr("%1 to %2").arg(amount, address);
            }
        }
        else if(!rcp.authenticatedMerchant.isEmpty()) // authenticated payment request
        {
            recipientElement = tr("%1 to %2").arg(amount, GUIUtil::HtmlEscape(rcp.authenticatedMerchant));
        }
        else // unauthenticated payment request
        {
            recipientElement = tr("%1 to %2").arg(amount, address);
        }

        formatted.append(recipientElement);
    }

    QString questionString = tr("Are you sure you want to send?");
    questionString.append("<br /><br />%1");

    if(txFee > 0)
    {
        // append fee string if a fee is required
        questionString.append("<hr /><span style='color:#aa0000;'>");
        questionString.append(BitcoinUnits::formatHtmlWithUnit(model->getOptionsModel()->getDisplayUnit(), txFee));
        questionString.append("</span> ");
        questionString.append(tr("added as transaction fee"));

        // append transaction size
        questionString.append(" (" + QString::number((double)currentTransaction.getTransactionSize() / 1000) + " kB)");
    }

    // add total amount in all subdivision units
    questionString.append("<hr />");
    CAmount totalAmount = currentTransaction.getTotalTransactionAmount() + txFee;
    QStringList alternativeUnits;
    for (BitcoinUnits::Unit u : BitcoinUnits::availableUnits())
    {
        if(u != model->getOptionsModel()->getDisplayUnit())
            alternativeUnits.append(BitcoinUnits::formatHtmlWithUnit(u, totalAmount));
    }
    questionString.append(tr("Total Amount %1")
        .arg(BitcoinUnits::formatHtmlWithUnit(model->getOptionsModel()->getDisplayUnit(), totalAmount)));
    questionString.append(QString("<span style='font-size:10pt;font-weight:normal;'><br />(=%1)</span>")
        .arg(alternativeUnits.join(" " + tr("or") + "<br />")));

    questionString.append("<hr /><span>");
    if (changeFeeDialog->ui->optInRBF->isChecked()) {
        questionString.append(tr("You can increase the fee later (signals Replace-By-Fee, BIP-125)."));
    } else {
        questionString.append(tr("Not signalling Replace-By-Fee, BIP-125."));
    }
    questionString.append("</span>");


    SendConfirmationDialog confirmationDialog(tr("Confirm send coins"),
        questionString.arg(formatted.join("<br />")), SEND_CONFIRM_DELAY, this);
    confirmationDialog.exec();
    QMessageBox::StandardButton retval = (QMessageBox::StandardButton)confirmationDialog.result();

    if(retval != QMessageBox::Yes)
    {
        fNewRecipientAllowed = true;
        return;
    }

    // now send the prepared transaction
    WalletModel::SendCoinsReturn sendStatus = model->sendCoins(currentTransaction);
    // process sendStatus and on error generate message shown to user
    processSendCoinsReturn(sendStatus);

    if (sendStatus.status == WalletModel::OK)
    {
        accept();
        CoinControlDialog::coinControl()->UnSelectAll();
        coinControlUpdateLabels();
    }
    fNewRecipientAllowed = true;
}

void SendCoinsDialog::clear()
{
    // Remove entries until only one left
    while(ui->entries->count())
    {
        ui->entries->takeAt(0)->widget()->deleteLater();
    }
    addEntry();

    updateTabsAndLabels();
}

void SendCoinsDialog::reject()
{
    clear();
}

void SendCoinsDialog::accept()
{
    clear();
}

SendCoinsEntry *SendCoinsDialog::addEntry()
{
    SendCoinsEntry *entry = new SendCoinsEntry(platformStyle, this);
    entry->setModel(model);
    ui->entries->addWidget(entry);
    connect(entry, SIGNAL(removeEntry(SendCoinsEntry*)), this, SLOT(removeEntry(SendCoinsEntry*)));
    connect(entry, SIGNAL(useAvailableBalance(SendCoinsEntry*)), this, SLOT(useAvailableBalance(SendCoinsEntry*)));
    connect(entry, SIGNAL(payAmountChanged()), this, SLOT(coinControlUpdateLabels()));
    connect(entry, SIGNAL(addressChanged()), this, SLOT(coinControlUpdateLabels()));
    connect(entry, SIGNAL(subtractFeeFromAmountChanged()), this, SLOT(coinControlUpdateLabels()));

    // Focus the field, so that entry can start immediately
    entry->clear();
    entry->setFocus();
    //ui->scrollAreaWidgetContents->resize(ui->scrollAreaWidgetContents->sizeHint());
    qApp->processEvents();
    //QScrollBar* bar = ui->scrollArea->verticalScrollBar();
    //if(bar)
    //    bar->setSliderPosition(bar->maximum());

    updateTabsAndLabels();
    return entry;
}

void SendCoinsDialog::updateTabsAndLabels()
{
    setupTabChain(0);
    coinControlUpdateLabels();
}

void SendCoinsDialog::removeEntry(SendCoinsEntry* entry)
{
    entry->hide();

    // If the last entry is about to be removed add an empty one
    if (ui->entries->count() == 1)
        addEntry();

    entry->deleteLater();

    updateTabsAndLabels();
}

QWidget *SendCoinsDialog::setupTabChain(QWidget *prev)
{
    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry)
        {
            prev = entry->setupTabChain(prev);
        }
    }
    QWidget::setTabOrder(prev, ui->sendButton);
    //QWidget::setTabOrder(ui->sendButton, ui->clearButton);
    //QWidget::setTabOrder(ui->clearButton, ui->addButton);
    //return ui->addButton;
    return ui->sendButton;
}

void SendCoinsDialog::setAddress(const QString &address)
{
    SendCoinsEntry *entry = 0;
    // Replace the first entry if it is still unused
    if(ui->entries->count() == 1)
    {
        SendCoinsEntry *first = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(0)->widget());
        if(first->isClear())
        {
            entry = first;
        }
    }
    if(!entry)
    {
        entry = addEntry();
    }

    entry->setAddress(address);
}

void SendCoinsDialog::pasteEntry(const SendCoinsRecipient &rv)
{
    if(!fNewRecipientAllowed)
        return;

    SendCoinsEntry *entry = 0;
    // Replace the first entry if it is still unused
    if(ui->entries->count() == 1)
    {
        SendCoinsEntry *first = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(0)->widget());
        if(first->isClear())
        {
            entry = first;
        }
    }
    if(!entry)
    {
        entry = addEntry();
    }

    entry->setValue(rv);
    updateTabsAndLabels();
}

bool SendCoinsDialog::handlePaymentRequest(const SendCoinsRecipient &rv)
{
    // Just paste the entry, all pre-checks
    // are done in paymentserver.cpp.
    pasteEntry(rv);
    return true;
}

void SendCoinsDialog::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance,
                                 const CAmount& watchBalance, const CAmount& watchUnconfirmedBalance, const CAmount& watchImmatureBalance)
{
    Q_UNUSED(unconfirmedBalance);
    Q_UNUSED(immatureBalance);
    Q_UNUSED(watchBalance);
    Q_UNUSED(watchUnconfirmedBalance);
    Q_UNUSED(watchImmatureBalance);

    if(model && model->getOptionsModel())
    {
        //QString balanceStr = BitcoinUnits::format(BitcoinUnits::Unit::BTC_rounded, balance);
        //int labelBalanceMaxWidth = ui->frameTotal2->width() - ui->labelBalanceUnit->width();
        //int fontSize = GUIUtil::getFontPixelSize(balanceStr, 5, 28, labelBalanceMaxWidth, QString("Roboto Mono"), 700);
        //QString labelBalanceStyle = "background-color: transparent; font-family: \"Roboto Mono\"; font-weight: 700; font-size: ";
        //labelBalanceStyle = labelBalanceStyle + QString(std::to_string(fontSize).c_str()) + QString("px;");
        //ui->labelBalance->setStyleSheet(labelBalanceStyle);
        //ui->labelBalance->setText(balanceStr);
    }
}

void SendCoinsDialog::updateDisplayUnit()
{
    setBalance(model->getBalance(), 0, 0, 0, 0, 0);
    changeFeeDialog->ui->customFee->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
    updateMinFeeLabel();
    updateSmartFeeLabel();
}

void SendCoinsDialog::processSendCoinsReturn(const WalletModel::SendCoinsReturn &sendCoinsReturn, const QString &msgArg)
{
    QPair<QString, CClientUIInterface::MessageBoxFlags> msgParams;
    // Default to a warning message, override if error message is needed
    msgParams.second = CClientUIInterface::MSG_WARNING;

    // This comment is specific to SendCoinsDialog usage of WalletModel::SendCoinsReturn.
    // WalletModel::TransactionCommitFailed is used only in WalletModel::sendCoins()
    // all others are used only in WalletModel::prepareTransaction()
    switch(sendCoinsReturn.status)
    {
    case WalletModel::InvalidAddress:
        msgParams.first = tr("The recipient address is not valid. Please recheck.");
        break;
    case WalletModel::InvalidAmount:
        msgParams.first = tr("The amount to pay must be larger than 0.");
        break;
    case WalletModel::AmountExceedsBalance:
        msgParams.first = tr("The amount exceeds your balance.");
        break;
    case WalletModel::AmountWithFeeExceedsBalance:
        msgParams.first = tr("The total exceeds your balance when the %1 transaction fee is included.").arg(msgArg);
        break;
    case WalletModel::DuplicateAddress:
        msgParams.first = tr("Duplicate address found: addresses should only be used once each.");
        break;
    case WalletModel::TransactionCreationFailed:
        msgParams.first = tr("Transaction creation failed!");
        msgParams.second = CClientUIInterface::MSG_ERROR;
        break;
    case WalletModel::TransactionCommitFailed:
        msgParams.first = tr("The transaction was rejected with the following reason: %1").arg(sendCoinsReturn.reasonCommitFailed);
        msgParams.second = CClientUIInterface::MSG_ERROR;
        break;
    case WalletModel::AbsurdFee:
        msgParams.first = tr("A fee higher than %1 is considered an absurdly high fee.").arg(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), maxTxFee));
        break;
    case WalletModel::PaymentRequestExpired:
        msgParams.first = tr("Payment request expired.");
        msgParams.second = CClientUIInterface::MSG_ERROR;
        break;
    // included to prevent a compiler warning.
    case WalletModel::OK:
    default:
        return;
    }

    Q_EMIT message(tr("Send Coins"), msgParams.first, msgParams.second);
}

void SendCoinsDialog::minimizeFeeSection(bool fMinimize)
{
    //ui->labelFeeMinimized->setVisible(fMinimize);
    //ui->buttonChooseFee  ->setVisible(fMinimize);
    //ui->buttonMinimizeFee->setVisible(!fMinimize);
    //ui->frameFeeSelection->setVisible(!fMinimize);
    //ui->horizontalLayoutSmartFee->setContentsMargins(0, (fMinimize ? 0 : 6), 0, 0);
    fFeeMinimized = fMinimize;
}

void SendCoinsDialog::on_buttonChooseFee_clicked()
{
    minimizeFeeSection(false);
}

void SendCoinsDialog::on_buttonMinimizeFee_clicked()
{
    updateFeeMinimizedLabel();
    minimizeFeeSection(true);
}

void SendCoinsDialog::useAvailableBalance(SendCoinsEntry* entry)
{
    // Get CCoinControl instance if CoinControl is enabled or create a new one.
    CCoinControl coin_control;
    if (model->getOptionsModel()->getCoinControlFeatures()) {
        coin_control = *CoinControlDialog::coinControl();
    }

    // Calculate available amount to send.
    CAmount amount = model->getBalance(&coin_control);
    for (int i = 0; i < ui->entries->count(); ++i) {
        SendCoinsEntry* e = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if (e && !e->isHidden() && e != entry) {
            amount -= e->getValue().amount;
        }
    }

    if (amount > 0) {
      entry->checkSubtractFeeFromAmount();
      entry->setAmount(amount);
    } else {
      entry->setAmount(0);
    }
}

void SendCoinsDialog::setMinimumFee()
{
    if (changeFeeDialog->ui->checkBoxMinimumFee->isChecked()) {
        changeFeeDialog->ui->customFee->setValue(GetRequiredFee(1000));
    }
}

void SendCoinsDialog::updateFeeSectionControls()
{
    changeFeeDialog->ui->confTargetSelector->setEnabled(changeFeeDialog->ui->radioSmartFee->isChecked());
    changeFeeDialog->ui->labelSmartFee->setEnabled(changeFeeDialog->ui->radioSmartFee->isChecked());
    changeFeeDialog->ui->labelSmartFee2->setEnabled(changeFeeDialog->ui->radioSmartFee->isChecked());
    //ui->labelSmartFee3          ->setEnabled(ui->radioSmartFee->isChecked());
    //ui->labelFeeEstimation      ->setEnabled(ui->radioSmartFee->isChecked());
    changeFeeDialog->ui->checkBoxMinimumFee->setEnabled(changeFeeDialog->ui->radioCustomFee->isChecked());
    changeFeeDialog->ui->checkBoxCustomFee->setEnabled(changeFeeDialog->ui->radioCustomFee->isChecked());
    //ui->labelMinFeeWarning      ->setEnabled(ui->radioCustomFee->isChecked());
    //ui->labelCustomPerKilobyte  ->setEnabled(ui->radioCustomFee->isChecked() && !ui->checkBoxMinimumFee->isChecked());
    changeFeeDialog->ui->customFee->setEnabled(changeFeeDialog->ui->radioCustomFee->isChecked() && !changeFeeDialog->ui->checkBoxMinimumFee->isChecked());
}

void SendCoinsDialog::updateFeeMinimizedLabel()
{
    if(!model || !model->getOptionsModel())
        return;

    if (changeFeeDialog->ui->radioSmartFee->isChecked()) {
    //    ui->labelFeeMinimized->setText(ui->labelSmartFee->text());
    } else {
    //    ui->labelFeeMinimized->setText(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), ui->customFee->value()) + "/kB");
    }
}

void SendCoinsDialog::updateMinFeeLabel()
{
    //if (model && model->getOptionsModel())
        //ui->checkBoxMinimumFee->setText(tr("Pay only the required fee of %1").arg(
        //    BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), GetRequiredFee(1000)) + "/kB")
        //);
}

void SendCoinsDialog::updateCoinControlState(CCoinControl& ctrl)
{
    if (changeFeeDialog->ui->radioCustomFee->isChecked()) {
        ctrl.m_feerate = CFeeRate(changeFeeDialog->ui->customFee->value());
    } else {
        ctrl.m_feerate.reset();
    }

    // Avoid using global defaults when sending money from the GUI
    // Either custom fee will be used or if not selected, the confirmation target from dropdown box
    ctrl.m_confirm_target = getConfTargetForIndex(changeFeeDialog->ui->confTargetSelector->value());

    ctrl.signalRbf = changeFeeDialog->ui->optInRBF->isChecked();
    ctrl.signalRbf = false;
}

void SendCoinsDialog::updateSmartFeeLabel()
{
    if(!model || !model->getOptionsModel())
        return;
    CCoinControl coin_control;
    updateCoinControlState(coin_control);
    coin_control.m_feerate.reset(); // Explicitly use only fee estimation rate for smart fee labels
    FeeCalculation feeCalc;
    CFeeRate feeRate = CFeeRate(GetMinimumFee(1000, coin_control, ::mempool, ::feeEstimator, &feeCalc));

    int n = getConfTargetForIndex(changeFeeDialog->ui->confTargetSelector->value());
    changeFeeDialog->ui->labelConf->setText(tr("%1 / %2 blocks").arg(GUIUtil::formatNiceTimeOffset(n*Params().GetConsensus().nPowTargetSpacing)).arg(n));

    changeFeeDialog->ui->labelSmartFee->setText(BitcoinUnits::format(model->getOptionsModel()->getDisplayUnit(), feeRate.GetFeePerK()));

    if (feeCalc.reason == FeeReason::FALLBACK) {
        changeFeeDialog->ui->labelSmartFee2->setText(tr("[ Smart fee not initialized yet. This usualy takes a few blocks ... ]"));
        //ui->labelSmartFee2->show(); // (Smart fee not initialized yet. This usually takes a few blocks...)
        //ui->labelFeeEstimation->setText("");
        //ui->fallbackFeeWarningLabel->setVisible(true);
        //int lightness = ui->fallbackFeeWarningLabel->palette().color(QPalette::WindowText).lightness();
        //QColor warning_colour(255 - (lightness / 5), 176 - (lightness / 3), 48 - (lightness / 14));
        //ui->fallbackFeeWarningLabel->setStyleSheet("QLabel { color: " + warning_colour.name() + "; }");
        //ui->fallbackFeeWarningLabel->setIndent(QFontMetrics(ui->fallbackFeeWarningLabel->font()).width("x"));
    }
    else
    {
        changeFeeDialog->ui->labelSmartFee2->setText(tr("Estimated to begin confirmation within %n block(s).", "", feeCalc.returnedTarget));
        //ui->labelSmartFee2->hide();
        //ui->labelFeeEstimation->setText(tr("Estimated to begin confirmation within %n block(s).", "", feeCalc.returnedTarget));
        //ui->fallbackFeeWarningLabel->setVisible(false);
    }

    updateFeeMinimizedLabel();
}

// Coin Control: copy label "Quantity" to clipboard
void SendCoinsDialog::coinControlClipboardQuantity()
{
    //GUIUtil::setClipboard(ui->labelCoinControlQuantity->text());
}

// Coin Control: copy label "Amount" to clipboard
void SendCoinsDialog::coinControlClipboardAmount()
{
    //GUIUtil::setClipboard(ui->labelCoinControlAmount->text().left(ui->labelCoinControlAmount->text().indexOf(" ")));
}

// Coin Control: copy label "Fee" to clipboard
void SendCoinsDialog::coinControlClipboardFee()
{
    //GUIUtil::setClipboard(ui->labelCoinControlFee->text().left(ui->labelCoinControlFee->text().indexOf(" ")).replace(ASYMP_UTF8, ""));
}

// Coin Control: copy label "After fee" to clipboard
void SendCoinsDialog::coinControlClipboardAfterFee()
{
    //GUIUtil::setClipboard(ui->labelCoinControlAfterFee->text().left(ui->labelCoinControlAfterFee->text().indexOf(" ")).replace(ASYMP_UTF8, ""));
}

// Coin Control: copy label "Bytes" to clipboard
void SendCoinsDialog::coinControlClipboardBytes()
{
    //GUIUtil::setClipboard(ui->labelCoinControlBytes->text().replace(ASYMP_UTF8, ""));
}

// Coin Control: copy label "Dust" to clipboard
void SendCoinsDialog::coinControlClipboardLowOutput()
{
    //GUIUtil::setClipboard(ui->labelCoinControlLowOutput->text());
}

// Coin Control: copy label "Change" to clipboard
void SendCoinsDialog::coinControlClipboardChange()
{
    //GUIUtil::setClipboard(ui->labelCoinControlChange->text().left(ui->labelCoinControlChange->text().indexOf(" ")).replace(ASYMP_UTF8, ""));
}

// Coin Control: settings menu - coin control enabled/disabled by user
void SendCoinsDialog::coinControlFeatureChanged(bool checked)
{
    //ui->frameCoinControl->setVisible(checked);

    if (!checked && model) // coin control features disabled
        CoinControlDialog::coinControl()->SetNull();

    coinControlUpdateLabels();
}

// Coin Control: button inputs -> show actual coin control dialog
void SendCoinsDialog::coinControlButtonClicked()
{
    CoinControlDialog dlg(platformStyle);
    dlg.setModel(model);
    dlg.exec();
    coinControlUpdateLabels();
}

// Coin Control: checkbox custom change address
void SendCoinsDialog::coinControlChangeChecked(int state)
{
    if (state == Qt::Unchecked)
    {
        CoinControlDialog::coinControl()->destChange = CNoDestination();
        //ui->labelCoinControlChangeLabel->clear();
    }
    //else
        // use this to re-validate an already entered address
        //coinControlChangeEdited(ui->lineEditCoinControlChange->text());

    //ui->lineEditCoinControlChange->setEnabled((state == Qt::Checked));
}

// Coin Control: custom change address changed
void SendCoinsDialog::coinControlChangeEdited(const QString& text)
{
    if (model && model->getAddressTableModel())
    {
        // Default to no change address until verified
        CoinControlDialog::coinControl()->destChange = CNoDestination();
        //ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color:red;}");

        const CTxDestination dest = DecodeDestination(text.toStdString());

        if (text.isEmpty()) // Nothing entered
        {
            //ui->labelCoinControlChangeLabel->setText("");
        }
        else if (!IsValidDestination(dest)) // Invalid address
        {
            //ui->labelCoinControlChangeLabel->setText(tr("Warning: Invalid Bitcoin address"));
        }
        else // Valid address
        {
            if (!model->IsSpendable(dest)) {
                //ui->labelCoinControlChangeLabel->setText(tr("Warning: Unknown change address"));

                // confirmation dialog
                QMessageBox::StandardButton btnRetVal = QMessageBox::question(this, tr("Confirm custom change address"), tr("The address you selected for change is not part of this wallet. Any or all funds in your wallet may be sent to this address. Are you sure?"),
                    QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

                if(btnRetVal == QMessageBox::Yes)
                    CoinControlDialog::coinControl()->destChange = dest;
                else
                {
                    //ui->lineEditCoinControlChange->setText("");
                    //ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color:black;}");
                    //ui->labelCoinControlChangeLabel->setText("");
                }
            }
            else // Known change address
            {
                //ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color:black;}");

                // Query label
                //QString associatedLabel = model->getAddressTableModel()->labelForAddress(text);
                //if (!associatedLabel.isEmpty())
                    //ui->labelCoinControlChangeLabel->setText(associatedLabel);
                //elsecoinControlUpdateLabels()
                    //ui->labelCoinControlChangeLabel->setText(tr("(no label)"));

                CoinControlDialog::coinControl()->destChange = dest;
            }
        }
    }
}

void SendCoinsDialog::updateCardInfo()
{
    QList<SendCoinsRecipient> recipients;

    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry)
        {
            recipients.append(entry->getValue());
        }
    }

    // prepare transaction for getting txFee earlier
    WalletModelTransaction currentTransaction(recipients);
    WalletModel::SendCoinsReturn prepareStatus;

    // Always use a CCoinControl instance, use the CoinControlDialog instance if CoinControl has been enabled
    CCoinControl ctrl;
    if (model->getOptionsModel()->getCoinControlFeatures())
        ctrl = *CoinControlDialog::coinControl();

    updateCoinControlState(ctrl);

    prepareStatus = model->prepareTransaction(currentTransaction, ctrl, false);

    if(prepareStatus.status != WalletModel::OK) {
        ui->labelCurFee->setStyleSheet("background-color: transparent; font-family: \"Roboto Mono\"; font-weight: 700; font-size: 13px;");
        ui->labelCurAmount->setStyleSheet("background-color: transparent; font-family: \"Roboto Mono\"; font-weight: 700; font-size: 28px;");
        ui->labelCurFee->setText("0.00000000");
        ui->labelCurAmount->setText("0.00000000");
        return;
    }

    CAmount txFee = currentTransaction.getTransactionFee();

    QString feeStr = BitcoinUnits::format(BitcoinUnits::Unit::BTC, txFee);
    int labelFeeMaxWidth = ui->frameFee->width() - ui->btnChangeFee->width() - 50;
    int fontSize1 = GUIUtil::getFontPixelSize(feeStr, 5, 13, labelFeeMaxWidth, QString("Roboto Mono"), 700);
    QString labelFeeStyle = "background-color: transparent; font-family: \"Roboto Mono\"; font-weight: 700; font-size: ";
    labelFeeStyle = labelFeeStyle + QString(std::to_string(fontSize1).c_str()) + QString("px;");
    ui->labelCurFee->setStyleSheet(labelFeeStyle);
    ui->labelCurFee->setText(feeStr);

    QString amountStr = BitcoinUnits::format(BitcoinUnits::Unit::BTC, currentTransaction.getTotalTransactionAmount() + txFee);
    int labelAmountMaxWidth = ui->frameTotal2->width() - ui->labelBalanceUnit->width();
    int fontSize2 = GUIUtil::getFontPixelSize(amountStr, 5, 28, labelAmountMaxWidth, QString("Roboto Mono"), 700);
    QString labelAmountStyle = "background-color: transparent; font-family: \"Roboto Mono\"; font-weight: 700; font-size: ";
    labelAmountStyle = labelAmountStyle + QString(std::to_string(fontSize2).c_str()) + QString("px;");
    ui->labelCurAmount->setStyleSheet(labelAmountStyle);
    ui->labelCurAmount->setText(amountStr);
}

// Coin Control: update labels
void SendCoinsDialog::coinControlUpdateLabels()
{   
    if (!model || !model->getOptionsModel())
        return;

    updateCoinControlState(*CoinControlDialog::coinControl());

    updateCardInfo();

    // set pay amounts
    CoinControlDialog::payAmounts.clear();
    CoinControlDialog::fSubtractFeeFromAmount = false;

    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry && !entry->isHidden())
        {
            SendCoinsRecipient rcp = entry->getValue();
            CoinControlDialog::payAmounts.append(rcp.amount);
            if (rcp.fSubtractFeeFromAmount)
                CoinControlDialog::fSubtractFeeFromAmount = true;
        }
    }

    if (CoinControlDialog::coinControl()->HasSelected())
    {
        // actual coin control calculation
        //CoinControlDialog::updateLabels(model, this);

        // show coin control stats
        //ui->labelCoinControlAutomaticallySelected->hide();
        //ui->widgetCoinControl->show();
    }
    else
    {
        // hide coin control stats
        //ui->labelCoinControlAutomaticallySelected->show();
        //ui->widgetCoinControl->hide();
        //ui->labelCoinControlInsuffFunds->hide();
    }
}

SendConfirmationDialog::SendConfirmationDialog(const QString &title, const QString &text, int _secDelay,
    QWidget *parent) :
    QMessageBox(QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::Cancel, parent), secDelay(_secDelay)
{
    setDefaultButton(QMessageBox::Cancel);
    yesButton = button(QMessageBox::Yes);
    updateYesButton();
    connect(&countDownTimer, SIGNAL(timeout()), this, SLOT(countDown()));
}

int SendConfirmationDialog::exec()
{
    updateYesButton();
    countDownTimer.start(1000);
    return QMessageBox::exec();
}

void SendConfirmationDialog::countDown()
{
    secDelay--;
    updateYesButton();

    if(secDelay <= 0)
    {
        countDownTimer.stop();
    }
}

void SendConfirmationDialog::updateYesButton()
{
    if(secDelay > 0)
    {
        yesButton->setEnabled(false);
        yesButton->setText(tr("Yes") + " (" + QString::number(secDelay) + ")");
    }
    else
    {
        yesButton->setEnabled(true);
        yesButton->setText(tr("Yes"));
    }
}
