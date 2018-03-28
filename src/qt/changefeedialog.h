#ifndef CHANGEFEEDIALOG_H
#define CHANGEFEEDIALOG_H

#include <QDialog>

namespace Ui {
class ChangeFeeDialog;
}

class ChangeFeeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChangeFeeDialog(QWidget *parent = 0);
    ~ChangeFeeDialog();

private Q_SLOTS:
    void on_closeBtn_clicked();

public:
    Ui::ChangeFeeDialog *ui;
};

#endif // CHANGEFEEDIALOG_H
