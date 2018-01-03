// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/walletframe.h>

#include <qt/bitcoingui.h>
#include <qt/walletview.h>
#include <qt/mainmenupanel.h>
#include <qt/stockinfo.h>

#include <cassert>
#include <cstdio>

#include <QHBoxLayout>
#include <QLabel>
#include <QFontDatabase>

WalletFrame::WalletFrame(const PlatformStyle *_platformStyle, BitcoinGUI *_gui) :
    QFrame(_gui),
    gui(_gui),
    stockInfo(nullptr),
    platformStyle(_platformStyle),
    mainMenuPanel(nullptr)
{
    QFontDatabase::addApplicationFont(":/fonts/RobotoMono-Bold");
    QFontDatabase::addApplicationFont(":/fonts/RobotoMono-BoldItalic");
    QFontDatabase::addApplicationFont(":/fonts/RobotoMono-Italic");
    QFontDatabase::addApplicationFont(":/fonts/RobotoMono-Light");
    QFontDatabase::addApplicationFont(":/fonts/RobotoMono-LightItalic");
    QFontDatabase::addApplicationFont(":/fonts/RobotoMono-Medium");
    QFontDatabase::addApplicationFont(":/fonts/RobotoMono-MediumItalic");
    QFontDatabase::addApplicationFont(":/fonts/RobotoMono-Regular");
    QFontDatabase::addApplicationFont(":/fonts/RobotoMono-Thin");
    QFontDatabase::addApplicationFont(":/fonts/RobotoMono-ThinItalic");

    setContentsMargins(0,0,0,0);

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0);
    mainLayout->setSpacing(0);
    setLayout(mainLayout);

    mainMenuPanel = new MainMenuPanel(this, platformStyle, this);

    walletStack = new QStackedWidget(this);
    walletStack->setContentsMargins(0,0,0,0);
    walletStack->setStyleSheet("background-color: #e8e8e8;");

    QLabel *noWallet = new QLabel(tr("No wallet has been loaded."));
    noWallet->setAlignment(Qt::AlignCenter);
    walletStack->addWidget(noWallet);

    mainLayout->addWidget(mainMenuPanel);
    mainLayout->addWidget(walletStack);

    stockInfo = new StockInfo(this);
}

WalletFrame::~WalletFrame()
{
    if (mainMenuPanel) {
        delete mainMenuPanel;
    }
}

void WalletFrame::setSyncProgress(double value, double max)
{
    QMap<QString, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->setSyncProgress(value, max);
}

void WalletFrame::setClientModel(ClientModel *_clientModel)
{
    this->clientModel = _clientModel;
}

bool WalletFrame::addWallet(const QString& name, WalletModel *walletModel)
{
    if (!gui || !clientModel || !walletModel || mapWalletViews.count(name) > 0)
        return false;

    WalletView *walletView = new WalletView(platformStyle, this);
    walletView->setBitcoinGUI(gui);
    walletView->setClientModel(clientModel);
    walletView->setWalletModel(walletModel);
    walletView->showOutOfSyncWarning(bOutOfSync);
    walletView->connectMainMenu(mainMenuPanel);
    walletView->addPriceWidget(stockInfo);

     /* TODO we should goto the currently selected page once dynamically adding wallets is supported */
    //walletView->gotoOverviewPage();
    walletStack->addWidget(walletView);
    mapWalletViews[name] = walletView;

    // Ensure a walletView is able to show the main window
    connect(walletView, SIGNAL(showNormalIfMinimized()), gui, SLOT(showNormalIfMinimized()));
    connect(walletView, SIGNAL(outOfSyncWarningClicked()), this, SLOT(outOfSyncWarningClicked()));

    mainMenuPanel->onWalletAdded();

    return true;
}

bool WalletFrame::setCurrentWallet(const QString& name)
{
    if (mapWalletViews.count(name) == 0)
        return false;

    WalletView *walletView = mapWalletViews.value(name);
    walletStack->setCurrentWidget(walletView);
    assert(walletView);
    walletView->updateEncryptionStatus();
    return true;
}

bool WalletFrame::removeWallet(const QString &name)
{
    if (mapWalletViews.count(name) == 0)
        return false;

    WalletView *walletView = mapWalletViews.take(name);
    walletStack->removeWidget(walletView);
    return true;
}

void WalletFrame::removeAllWallets()
{
    QMap<QString, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        walletStack->removeWidget(i.value());
    mapWalletViews.clear();
}

bool WalletFrame::handlePaymentRequest(const SendCoinsRecipient &recipient)
{
    WalletView *walletView = currentWalletView();
    if (!walletView)
        return false;

    return walletView->handlePaymentRequest(recipient);
}

void WalletFrame::showOutOfSyncWarning(bool fShow)
{
    bOutOfSync = fShow;
    QMap<QString, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->showOutOfSyncWarning(fShow);
}

void WalletFrame::gotoOverviewPage()
{
    QMap<QString, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->gotoOverviewPage();

    if (mainMenuPanel) {
        mainMenuPanel->onLoadedOverviewPage();
    }
}

void WalletFrame::gotoHistoryPage()
{
    QMap<QString, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->gotoHistoryPage();

    if (mainMenuPanel) {
        mainMenuPanel->onLoadedTransactionPage();
    }
}

void WalletFrame::gotoReceiveCoinsPage()
{
    QMap<QString, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->gotoReceiveCoinsPage();

    if (mainMenuPanel) {
        mainMenuPanel->onLoadedReceivePage();
    }
}

void WalletFrame::gotoSendCoinsPage(QString addr)
{
    QMap<QString, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->gotoSendCoinsPage(addr);

    if (mainMenuPanel) {
        mainMenuPanel->onLoadedSendPage();
    }
}

void WalletFrame::gotoSignMessageTab(QString addr)
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->gotoSignMessageTab(addr);
}

void WalletFrame::gotoVerifyMessageTab(QString addr)
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->gotoVerifyMessageTab(addr);
}

void WalletFrame::encryptWallet(bool status)
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->encryptWallet(status);
}

void WalletFrame::backupWallet()
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->backupWallet();
}

void WalletFrame::changePassphrase()
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->changePassphrase();
}

void WalletFrame::unlockWallet()
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->unlockWallet();
}

void WalletFrame::usedSendingAddresses()
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->usedSendingAddresses();
}

void WalletFrame::usedReceivingAddresses()
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->usedReceivingAddresses();
}

WalletView *WalletFrame::currentWalletView()
{
    return qobject_cast<WalletView*>(walletStack->currentWidget());
}

void WalletFrame::outOfSyncWarningClicked()
{
    Q_EMIT requestedSyncWarningInfo();
}
