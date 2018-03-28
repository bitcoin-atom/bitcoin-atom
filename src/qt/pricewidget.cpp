#include <qt/pricewidget.h>
#include <qt/forms/ui_pricewidget.h>
#include <qt/pricewidget.moc>
#include <qt/stockinfo.h>

PriceWidget::PriceWidget(StockInfo *stockInfo, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PriceWidget),
    info(stockInfo)
{
    ui->setupUi(this);
    setVisible(false);
    stockInfo->subscribe(std::bind(&PriceWidget::onChangeInfo, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

PriceWidget::~PriceWidget()
{
    delete ui;
}

void PriceWidget::onChangeInfo(const QString& price, const QString& percent, const QString& volume)
{
    setVisible(true);
    QString volumeStr = QString("$") + volume;
    QString absPercent = percent;
    if (percent.at(0) == '-') {
        absPercent.remove(0, 1);
        ui->labelPercent->setStyleSheet("font-family: \"Roboto Mono\"; font-size: 13px; font-weight: 700; color: #c53f4f; padding-left: 6px;");
        ui->counterPrice->setStyleSheet("background-color: transparent; border-image: url(:/icons/triangle_red);");
    } else {
        ui->labelPercent->setStyleSheet("font-family: \"Roboto Mono\"; font-size: 13px; font-weight: 700; color: #4f9600; padding-left: 6px;");
        ui->counterPrice->setStyleSheet("background-color: transparent; border-image: url(:/icons/triangle_green);");
    }
    QString priceStr = QString("$") + price;
    QString percentStr = absPercent + QString("%");
    ui->labelPrice->setText(priceStr);
    ui->labelVolume->setText(volumeStr);
    ui->labelPercent->setText(percentStr);
}
