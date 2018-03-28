#include "stockinfo.h"
#include <qt/stockinfo.moc>

#include <QWidget>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QByteArray>
#include <QNetworkRequest>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonObject>
#include <QSslConfiguration>

StockInfo::StockInfo(QWidget *parent) :
    QObject(parent),
    timer(nullptr),
    network(nullptr)
{
    timer = new QTimer(this);
    timer->setInterval(60000);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateTimer()));

    network = new QNetworkAccessManager(this);
    connect(network, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyFinished(QNetworkReply*)));
}

StockInfo::~StockInfo()
{

}

void StockInfo::subscribe(ChangeInfoCallbackFunc onChangeInfoCallback)
{
    onChangeInfoCallbacks.push_back(onChangeInfoCallback);
    requestInfo();
}

void StockInfo::onChangeInfo(const QString& price, const QString& percent, const QString& volume)
{
    for (auto cb : onChangeInfoCallbacks) {
        if (cb) {
            cb(price, percent, volume);
        }
    }
}

void StockInfo::updateTimer()
{
    timer->stop();
    requestInfo();
}

void StockInfo::requestInfo()
{
    QNetworkRequest request;

    QSslConfiguration config = QSslConfiguration::defaultConfiguration();
    config.setProtocol(QSsl::TlsV1_2);
    request.setSslConfiguration(config);
    request.setUrl(QUrl("https://api.coinmarketcap.com/v1/ticker/bitcoin-atom/"));
    request.setHeader(QNetworkRequest::ServerHeader, "application/json");

    network->get(request);
}

void StockInfo::replyFinished(QNetworkReply *reply)
{
    QByteArray answer = reply->readAll();

    QJsonDocument doc = QJsonDocument::fromJson(answer);
    if (doc.isArray()) {
        QJsonArray all = doc.array();
        if (!all.isEmpty()) {
            QJsonValue val = all.first();
            QJsonObject info = val.toObject();
            QString price = info["price_usd"].toString();
            QString percent = info["percent_change_1h"].toString();
            QString volume = info["24h_volume_usd"].toString();
            onChangeInfo(price, percent, volume);
        }
    }

    timer->start();
}
