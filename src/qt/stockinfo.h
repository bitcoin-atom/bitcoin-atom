#ifndef STOCKINFO_H
#define STOCKINFO_H

#include <QObject>
#include <QString>

#include <functional>
#include <vector>

class QTimer;
class QNetworkAccessManager;
class QNetworkReply;

typedef std::function<void(const QString&, const QString&, const QString&)> ChangeInfoCallbackFunc;

class QWidget;

class StockInfo : public QObject
{
    Q_OBJECT
public:
    explicit StockInfo(QWidget *parent);
    ~StockInfo();
    void subscribe(ChangeInfoCallbackFunc onChangeInfoCallback);
private:
    void onChangeInfo(const QString& price, const QString& percent, const QString& volume);
    void requestInfo();
private Q_SLOTS:
    void updateTimer();
    void replyFinished(QNetworkReply *reply);
private:
    std::vector<ChangeInfoCallbackFunc> onChangeInfoCallbacks;
    QTimer *timer;
    QNetworkAccessManager* network;
};

#endif // STOCKINFO_H
