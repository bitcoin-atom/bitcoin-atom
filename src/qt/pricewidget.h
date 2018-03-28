#ifndef PRICEWIDGET_H
#define PRICEWIDGET_H

#include <QWidget>

namespace Ui {
class PriceWidget;
}

class StockInfo;

class PriceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PriceWidget(StockInfo *stockInfo, QWidget *parent = 0);
    ~PriceWidget();
private:
    void onChangeInfo(const QString& price, const QString& percent, const QString& volume);
private:
    Ui::PriceWidget *ui;
    StockInfo *info;
};

#endif // PRICEWIDGET_H
