#ifndef MAINMENUPANEL_H
#define MAINMENUPANEL_H

#include <QWidget>

class PlatformStyle;

namespace Ui {
class MainMenuPanel;
}

class QToolButton;
class WalletFrame;

class MainMenuPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MainMenuPanel(WalletFrame *walletFrame, const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~MainMenuPanel();
public:
    void onWalletAdded();
    void onSendClick();
    void onTransactionsClick();
private Q_SLOTS:
    void gotoOverviewPage();
    void gotoHistoryPage();
    void gotoReceiveCoinsPage();
    void gotoSendCoinsPage(QString addr = "");
private:
    Ui::MainMenuPanel *ui;
    QToolButton *curCheckedBtn;
    WalletFrame *walletFrame_;
};

#endif // MAINMENUPANEL_H
