#include <qt/mainmenupanel.h>
#include <qt/forms/ui_mainmenupanel.h>
#include <qt/platformstyle.h>
#include <qt/mainmenupanel.moc>
#include <qt/walletframe.h>
#include <clientversion.h>

MainMenuPanel::MainMenuPanel(WalletFrame *walletFrame, const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MainMenuPanel),
    curCheckedBtn(nullptr),
    walletFrame_(walletFrame)
{
    ui->setupUi(this);

    ui->btnOverview->setIcon(QIcon(":/icons/overview_icon_new"));
    ui->btnOverview->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    ui->btnSend->setIcon(QIcon(":/icons/send_icon_new"));
    ui->btnSend->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    ui->btnReceive->setIcon(QIcon(":/icons/receive_icon_new"));
    ui->btnReceive->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    ui->btnTransactions->setIcon(QIcon(":/icons/trans_icon_new"));
    ui->btnTransactions->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    connect(ui->btnOverview, SIGNAL(clicked()), this, SLOT(gotoOverviewPage()));
    connect(ui->btnSend, SIGNAL(clicked()), this, SLOT(gotoSendCoinsPage()));
    connect(ui->btnReceive, SIGNAL(clicked()), this, SLOT(gotoReceiveCoinsPage()));
    connect(ui->btnTransactions, SIGNAL(clicked()), this, SLOT(gotoHistoryPage()));

    ui->labelVersion->setText(QString::fromStdString(FormatVersionString("v.", CLIENT_VERSION)));
}

MainMenuPanel::~MainMenuPanel()
{
    delete ui;
}

void MainMenuPanel::onSendClick()
{
    gotoSendCoinsPage();
}

void MainMenuPanel::onTransactionsClick()
{
    gotoHistoryPage();
}

void MainMenuPanel::gotoOverviewPage()
{
    curPage = cpOverview;
    ui->btnSend->setChecked(false);
    ui->btnReceive->setChecked(false);
    ui->btnTransactions->setChecked(false);
    ui->btnOverview->setChecked(true);
    if (curCheckedBtn != ui->btnOverview) {
        curCheckedBtn = ui->btnOverview;
        walletFrame_->gotoOverviewPage();
    }
}

void MainMenuPanel::gotoHistoryPage()
{
    curPage = cpTransaction;
    ui->btnOverview->setChecked(false);
    ui->btnSend->setChecked(false);
    ui->btnReceive->setChecked(false);
    ui->btnTransactions->setChecked(true);
    if (curCheckedBtn != ui->btnTransactions) {
        curCheckedBtn = ui->btnTransactions;
        walletFrame_->gotoHistoryPage();
    }
}

void MainMenuPanel::gotoReceiveCoinsPage()
{
    curPage = cpReceive;
    ui->btnOverview->setChecked(false);
    ui->btnSend->setChecked(false);
    ui->btnTransactions->setChecked(false);
    ui->btnReceive->setChecked(true);
    if (curCheckedBtn != ui->btnReceive) {
        curCheckedBtn = ui->btnReceive;
        walletFrame_->gotoReceiveCoinsPage();
    }
}

void MainMenuPanel::gotoSendCoinsPage(QString addr)
{
    curPage = cpSend;
    ui->btnOverview->setChecked(false);
    ui->btnReceive->setChecked(false);
    ui->btnTransactions->setChecked(false);
    ui->btnSend->setChecked(true);
    if (curCheckedBtn != ui->btnSend) {
        curCheckedBtn = ui->btnSend;
        walletFrame_->gotoSendCoinsPage();
    }
}

void MainMenuPanel::onWalletAdded()
{
    gotoOverviewPage();
}

void MainMenuPanel::onLoadedOverviewPage()
{
    if (curPage == cpOverview) {
        return;
    }

    ui->btnOverview->setChecked(true);
    ui->btnSend->setChecked(false);
    ui->btnTransactions->setChecked(false);
    ui->btnReceive->setChecked(false);

    curCheckedBtn = ui->btnOverview;
}

void MainMenuPanel::onLoadedSendPage()
{
    if (curPage == cpSend) {
        return;
    }

    ui->btnOverview->setChecked(false);
    ui->btnSend->setChecked(true);
    ui->btnTransactions->setChecked(false);
    ui->btnReceive->setChecked(false);

    curCheckedBtn = ui->btnSend;
}

void MainMenuPanel::onLoadedReceivePage()
{
    if (curPage == cpReceive) {
        return;
    }

    ui->btnOverview->setChecked(false);
    ui->btnSend->setChecked(false);
    ui->btnTransactions->setChecked(false);
    ui->btnReceive->setChecked(true);

    curCheckedBtn = ui->btnReceive;
}

void MainMenuPanel::onLoadedTransactionPage()
{
    if (curPage == cpTransaction) {
        return;
    }

    ui->btnOverview->setChecked(false);
    ui->btnSend->setChecked(false);
    ui->btnTransactions->setChecked(true);
    ui->btnReceive->setChecked(false);

    curCheckedBtn = ui->btnTransactions;
}
