#include <qt/changefeedialog.h>
#include <qt/forms/ui_changefeedialog.h>
#include <qt/changefeedialog.moc>

ChangeFeeDialog::ChangeFeeDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ChangeFeeDialog)
{
    ui->setupUi(this);

    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);

    ui->closeBtn->setIcon(QIcon(":/icons/close_ico"));
}

ChangeFeeDialog::~ChangeFeeDialog()
{
    delete ui;
}

void ChangeFeeDialog::on_closeBtn_clicked()
{
    close();
}
