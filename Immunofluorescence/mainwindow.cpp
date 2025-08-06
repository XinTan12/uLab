#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->messageEdit->clear();
    isAirPumpStarted = false;
    pULab = new ULab(this);
    EnableAction(false);

    connect(pULab, &ULab::SendMessage, this, &MainWindow::ReceiveMessage);
    connect(pULab, &ULab::UpdatePos, this, &MainWindow::SlotUpdatePos);
    connect(pULab, &ULab::UpdatePressure, this, &MainWindow::SlotUpdatePressure);
    connect(pULab, &ULab::UpdateFlow, this, &MainWindow::SlotUpdateFlow);
    on_refreshButton_clicked();

    valveButtonGroup.addButton(ui->valve1RadioButton, 0);
    valveButtonGroup.addButton(ui->valve2RadioButton, 1);
    valveButtonGroup.addButton(ui->valve3RadioButton, 2);
    valveButtonGroup.addButton(ui->valve4RadioButton, 3);
    valveButtonGroup.addButton(ui->valve5RadioButton, 4);
    valveButtonGroup.addButton(ui->valve6RadioButton, 5);
    valveButtonGroup.addButton(ui->valve7RadioButton, 6);
    valveButtonGroup.addButton(ui->valve8RadioButton, 7);

    valveButtonGroup.setExclusive(false);
    valveStates = 0;

    connect(&valveButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(valveButtonGroupClicked(int)));
}

MainWindow::~MainWindow()
{
    pULab->disconnect();
    delete ui;
    delete pULab;
}

void MainWindow::SlotUpdatePos(DEVICE_CODE id, AXIS axis, int pos)
{
    if (id == LOW_STAGE_CODE)
    {
        switch (axis)
        {
            case AXIS_X: ui->xAxisPosSpinBox->setValue(pos / 500.0);break;
            case AXIS_Y: ui->yAxisPosSpinBox->setValue(pos / 500.0);break;
            case AXIS_Z: ui->zAxisPosSpinBox->setValue(pos / 500.0);break;
            default:;
        }
    }
    else
    {
        switch (axis)
        {
            case AXIS_X: ui->xAxisPosSpinBoxHigh->setValue(pos / 3200.0);break;
            case AXIS_Y: ui->yAxisPosSpinBoxHigh->setValue(pos / 3200.0);break;
            case AXIS_Z: ui->zAxisPosSpinBoxHigh->setValue(pos / 10.0);break;
            default:;
        }
    }
}

void MainWindow::SlotUpdatePressure(uint pressure)
{
    ui->pressureShowBox->setValue(pressure / 50.0);
}

void MainWindow::SlotUpdateFlow(uint flow)
{
    ui->flowShowBox->setValue(flow / 100.0);
}

void MainWindow::ReceiveMessage(QString msg)
{
    ui->messageEdit->append(msg);
}

void MainWindow::EnableAction(bool enable)
{
    ui->tabWidget->setEnabled(enable);
//    if (enable)
//    {
//        ui->xAxisLowBox->setEnabled(ui->xAxisLowEnableBox->isChecked());
//        ui->yAxisLowBox->setEnabled(ui->yAxisLowEnableBox->isChecked());
//        ui->zAxisLowBox->setEnabled(ui->zAxisLowEnableBox->isChecked());
//    }
}

void MainWindow::on_refreshButton_clicked()
{
    ui->commComboBox->clear();
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        QSerialPort serial;
        serial.setPort(info);
        if (serial.open(QIODevice::ReadWrite))
        {
            ui->commComboBox->addItem(info.portName());
            serial.close();
        }
    }
}

void MainWindow::on_openButton_clicked()
{
    QString portName = ui->commComboBox->currentText();
    if (!portName.isEmpty())
    {
        if (pULab->InitPort(portName))
        {
            EnableAction(true);
            ui->closeButton->setEnabled(true);
            ui->openButton->setEnabled(false);
        }
    }
    else
    {
        ReceiveMessage("Warning : COMM is Empty!");
    }
}

void MainWindow::on_closeButton_clicked()
{
    pULab->ClosePort();
    ui->openButton->setEnabled(true);
    ui->closeButton->setEnabled(false);
    EnableAction(false);
}

void MainWindow::on_valve1GoButton_clicked()
{
    pULab->GotoHole(ui->addr1Box->value(), ui->hole1ComboBox->currentText().toUInt(), ui->pump1idSpinBox->value());
}

void MainWindow::on_valve2GoButton_clicked()
{
    pULab->GotoHole(ui->addr2Box->value(), ui->hole2ComboBox->currentText().toUInt(), ui->pump1idSpinBox->value());
}


void MainWindow::on_pump2VelocityNumBox_editingFinished()
{
    pULab->SetSpeed(ui->pump2VelocityNumBox->value(), ui->pump2idSpinBox->value());
}

void MainWindow::on_pump2StartButton_clicked()
{
    pULab->Rotate(true, ui->pump2DirectionComboBox->currentIndex() == 0, ui->pump2idSpinBox->value());
    ui->pump2StopButton->setEnabled(true);
    ui->pump2StartButton->setEnabled(false);
}

void MainWindow::on_pump2StopButton_clicked()
{
    pULab->Rotate(false, true, ui->pump2idSpinBox->value());
    ui->pump2StopButton->setEnabled(false);
    ui->pump2StartButton->setEnabled(true);
}

void MainWindow::on_pump1VelocityNumBox_editingFinished()
{
    pULab->SetSpeed(ui->pump1VelocityNumBox->value(), ui->pump1idSpinBox->value());
}

void MainWindow::on_pump1StartButton_clicked()
{
    pULab->Rotate(true, ui->pump1DirectionComboBox->currentIndex() == 0, ui->pump1idSpinBox->value());
    ui->pump1StopButton->setEnabled(true);
    ui->pump1StartButton->setEnabled(false);
}

void MainWindow::on_pump1StopButton_clicked()
{
    pULab->Rotate(false, true, ui->pump1idSpinBox->value());
    ui->pump1StopButton->setEnabled(false);
    ui->pump1StartButton->setEnabled(true);
}

void MainWindow::on_xAxisLowGotoButton_clicked()
{
    pULab->Goto(AXIS_X, qRound(ui->xAxisDesSpinBox->value() * 500), LOW_STAGE_CODE);
}

void MainWindow::on_xAxisLowLeftButton_clicked()
{
    pULab->SetSpeedStage(AXIS_X, qRound(ui->xAxisSpeedSpinBox->value() / 0.12), LOW_STAGE_CODE);
//    MSleep(50);
    pULab->SetTime(AXIS_X, qRound(ui->xAxisTimeSpinBox->value() * 1000), LOW_STAGE_CODE);
//    MSleep(50);
    pULab->Go(AXIS_X, false, LOW_STAGE_CODE);
}

void MainWindow::on_xAxisLowHomeButton_clicked()
{
    pULab->Home(AXIS_X, LOW_STAGE_CODE);
}

void MainWindow::on_xAxisLowRightButton_clicked()
{
    pULab->SetSpeedStage(AXIS_X, qRound(ui->xAxisSpeedSpinBox->value() / 0.12), LOW_STAGE_CODE);
//    MSleep(50);
    pULab->SetTime(AXIS_X, qRound(ui->xAxisTimeSpinBox->value() * 1000), LOW_STAGE_CODE);
//    MSleep(50);
    pULab->Go(AXIS_X, true, LOW_STAGE_CODE);
}

//void MainWindow::on_xAxisLowEnableBox_stateChanged(int arg1)
//{
//    ui->xAxisLowBox->setEnabled(arg1);
//    pULab->SetAxisEnable(AXIS_X, arg1);
//}

void MainWindow::on_yAxisLowGotoButton_clicked()
{
    pULab->Goto(AXIS_Y, qRound(ui->yAxisDesSpinBox->value() * 500), LOW_STAGE_CODE);
}

void MainWindow::on_yAxisLowLeftButton_clicked()
{
    pULab->SetSpeedStage(AXIS_Y, qRound(ui->yAxisSpeedSpinBox->value() / 0.12), LOW_STAGE_CODE);
//    MSleep(50);
    pULab->SetTime(AXIS_Y, qRound(ui->yAxisTimeSpinBox->value() * 1000), LOW_STAGE_CODE);
//    MSleep(50);
    pULab->Go(AXIS_Y, false, LOW_STAGE_CODE);
}

void MainWindow::on_yAxisLowHomeButton_clicked()
{
    pULab->Home(AXIS_Y, LOW_STAGE_CODE);
}

void MainWindow::on_yAxisLowRightButton_clicked()
{
    pULab->SetSpeedStage(AXIS_Y, qRound(ui->yAxisSpeedSpinBox->value() / 0.12), LOW_STAGE_CODE);
//    MSleep(50);
    pULab->SetTime(AXIS_Y, qRound(ui->yAxisTimeSpinBox->value() * 1000), LOW_STAGE_CODE);
//    MSleep(50);
    pULab->Go(AXIS_Y, true, LOW_STAGE_CODE);
}

//void MainWindow::on_yAxisLowEnableBox_stateChanged(int arg1)
//{
//    ui->yAxisLowBox->setEnabled(arg1);
//    pULab->SetAxisEnable(AXIS_Y, arg1);
//}

void MainWindow::on_zAxisLowGotoButton_clicked()
{
    pULab->Goto(AXIS_Z, qRound(ui->zAxisDesSpinBox->value() * 500), LOW_STAGE_CODE);
}

void MainWindow::on_zAxisLowLeftButton_clicked()
{
    pULab->SetSpeedStage(AXIS_Z, qRound(ui->zAxisSpeedSpinBox->value() / 0.12), LOW_STAGE_CODE);
//    MSleep(50);
    pULab->SetTime(AXIS_Z, qRound(ui->zAxisTimeSpinBox->value() * 1000), LOW_STAGE_CODE);
//    MSleep(50);
    pULab->Go(AXIS_Z, false, LOW_STAGE_CODE);
}

void MainWindow::on_zAxisLowHomeButton_clicked()
{
    pULab->Home(AXIS_Z, LOW_STAGE_CODE);
}

void MainWindow::on_zAxisLowRightButton_clicked()
{
    pULab->SetSpeedStage(AXIS_Z, qRound(ui->zAxisSpeedSpinBox->value() / 0.12), LOW_STAGE_CODE);
//    MSleep(50);
    pULab->SetTime(AXIS_Z, qRound(ui->zAxisTimeSpinBox->value() * 1000), LOW_STAGE_CODE);
//    MSleep(50);
    pULab->Go(AXIS_Z, true, LOW_STAGE_CODE);
}

//void MainWindow::on_zAxisLowEnableBox_stateChanged(int arg1)
//{
//    ui->zAxisLowBox->setEnabled(arg1);
//    pULab->SetAxisEnable(AXIS_Z, arg1);
//}

void MainWindow::on_xAxisLowBox_clicked(bool checked)
{
    pULab->SetAxisEnable(AXIS_X, checked);
    ReceiveMessage(QString("X-axis Low-precision stage has been ") + (checked ? "enabled" : "disabled"));
}

void MainWindow::on_yAxisLowBox_clicked(bool checked)
{
    pULab->SetAxisEnable(AXIS_Y, checked);
    ReceiveMessage(QString("Y-axis Low-precision stage has been ") + (checked ? "enabled" : "disabled"));
}

void MainWindow::on_zAxisLowBox_clicked(bool checked)
{
    pULab->SetAxisEnable(AXIS_Z, checked);
    ReceiveMessage(QString("Z-axis Low-precision stage has been ") + (checked ? "enabled" : "disabled"));
}

void MainWindow::on_xAxisHighGotoButton_clicked()
{
    pULab->Goto(AXIS_X, qRound(ui->xAxisDesSpinBoxHigh->value() * 3200), HIGH_STAGE_CODE);
}

void MainWindow::on_xAxisHighLeftButton_clicked()
{
    pULab->SetSpeedStage(AXIS_X, qRound(ui->xAxisSpeedSpinBoxHigh->value() / 0.01), HIGH_STAGE_CODE);
//    MSleep(50);
    pULab->SetTime(AXIS_X, qRound(ui->xAxisTimeSpinBoxHigh->value() * 1000), HIGH_STAGE_CODE);
//    MSleep(50);
    pULab->Go(AXIS_X, true, HIGH_STAGE_CODE);
}

void MainWindow::on_xAxisHighHomeButton_clicked()
{
    pULab->Home(AXIS_X, HIGH_STAGE_CODE);
}

void MainWindow::on_xAxisHighRightButton_clicked()
{
    pULab->SetSpeedStage(AXIS_X, qRound(ui->xAxisSpeedSpinBoxHigh->value() / 0.01), HIGH_STAGE_CODE);
//    MSleep(50);
    pULab->SetTime(AXIS_X, qRound(ui->xAxisTimeSpinBoxHigh->value() * 1000), HIGH_STAGE_CODE);
//    MSleep(50);
    pULab->Go(AXIS_X, false, HIGH_STAGE_CODE);
}

//void MainWindow::on_xAxisHighEnableBox_stateChanged(int arg1)
//{
//    ui->xAxisHighBox->setEnabled(arg1);
//    pULab->SetAxisEnable(AXIS_X, arg1);
//}

void MainWindow::on_yAxisHighGotoButton_clicked()
{
    pULab->Goto(AXIS_Y, qRound(ui->yAxisDesSpinBoxHigh->value() * 3200), HIGH_STAGE_CODE);
}

void MainWindow::on_yAxisHighLeftButton_clicked()
{
    pULab->SetSpeedStage(AXIS_Y, qRound(ui->yAxisSpeedSpinBoxHigh->value() / 0.01), HIGH_STAGE_CODE);
//    MSleep(50);
    pULab->SetTime(AXIS_Y, qRound(ui->yAxisTimeSpinBoxHigh->value() * 1000), HIGH_STAGE_CODE);
//    MSleep(50);
    pULab->Go(AXIS_Y, true, HIGH_STAGE_CODE);
}

void MainWindow::on_yAxisHighHomeButton_clicked()
{
    pULab->Home(AXIS_Y, HIGH_STAGE_CODE);
}

void MainWindow::on_yAxisHighRightButton_clicked()
{
    pULab->SetSpeedStage(AXIS_Y, qRound(ui->yAxisSpeedSpinBoxHigh->value() / 0.01), HIGH_STAGE_CODE);
//    MSleep(50);
    pULab->SetTime(AXIS_Y, qRound(ui->yAxisTimeSpinBoxHigh->value() * 1000), HIGH_STAGE_CODE);
//    MSleep(50);
    pULab->Go(AXIS_Y, false, HIGH_STAGE_CODE);
}

//void MainWindow::on_yAxisHighEnableBox_stateChanged(int arg1)
//{
//    ui->yAxisHighBox->setEnabled(arg1);
//    pULab->SetAxisEnable(AXIS_Y, arg1);
//}

void MainWindow::on_zAxisHighGotoButton_clicked()
{
    pULab->Goto(AXIS_Z, qRound(ui->zAxisDesSpinBoxHigh->value() * 10), HIGH_STAGE_CODE);
}

void MainWindow::on_zAxisHighLeftButton_clicked()
{
    pULab->SetSpeedStage(AXIS_Z, qRound(ui->zAxisSpeedSpinBoxHigh->value() / 3.0), HIGH_STAGE_CODE);
//    MSleep(50);
    pULab->SetTime(AXIS_Z, qRound(ui->zAxisTimeSpinBoxHigh->value() * 1000), HIGH_STAGE_CODE);
//    MSleep(50);
    pULab->Go(AXIS_Z, true, HIGH_STAGE_CODE);
}

void MainWindow::on_zAxisHighHomeButton_clicked()
{
    pULab->Home(AXIS_Z, HIGH_STAGE_CODE);
}

void MainWindow::on_zAxisHighRightButton_clicked()
{
    pULab->SetSpeedStage(AXIS_Z, qRound(ui->zAxisSpeedSpinBoxHigh->value() / 3.0), HIGH_STAGE_CODE);
//    MSleep(50);
    pULab->SetTime(AXIS_Z, qRound(ui->zAxisTimeSpinBoxHigh->value() * 1000), HIGH_STAGE_CODE);
//    MSleep(50);
    pULab->Go(AXIS_Z, false, HIGH_STAGE_CODE);
}

//void MainWindow::on_zAxisHighEnableBox_stateChanged(int arg1)
//{
//    ui->zAxisHighBox->setEnabled(arg1);
//    pULab->SetAxisEnable(AXIS_Z, arg1);
//}

void MainWindow::on_xAxisHighBox_clicked(bool checked)
{
    pULab->SetAxisEnable(AXIS_X, checked, HIGH_STAGE_CODE);
    ReceiveMessage(QString("X-axis High-precision stage has been ") + (checked ? "enabled" : "disabled"));
}

void MainWindow::on_yAxisHighBox_clicked(bool checked)
{
    pULab->SetAxisEnable(AXIS_Y, checked, HIGH_STAGE_CODE);
    ReceiveMessage(QString("Y-axis High-precision stage has been ") + (checked ? "enabled" : "disabled"));
}

void MainWindow::on_zAxisHighBox_clicked(bool checked)
{
    pULab->SetAxisEnable(AXIS_Z, checked, HIGH_STAGE_CODE);
    ReceiveMessage(QString("Z-axis High-precision stage has been ") + (checked ? "enabled" : "disabled"));
}

void MainWindow::on_airPumpStartButton_clicked()
{
    if (isAirPumpStarted)
    {
        pULab->StopPump();
        ui->airPumpStartButton->setText("Start");
        isAirPumpStarted = false;
    }
    else
    {
        pULab->StartPump(ui->airPumpVelocityNumBox->value());
        ui->airPumpStartButton->setText("Stop");
        isAirPumpStarted = true;
    }
}

void MainWindow::on_pressureSetButton_clicked()
{
    pULab->SetPressure(qRound(ui->pressureSetBox->value() * 50));
}

void MainWindow::on_flowSetButton_clicked()
{
    pULab->SetFlow(qRound(ui->flowSetBox->value() * 100));
}

void MainWindow::on_FluigentBox_clicked(bool checked)
{
    pULab->SetFluigentEnable(checked);
}

void MainWindow::valveButtonGroupClicked(int id)
{
    uint8_t temp = 0x01 << (7 - id);
    valveStates ^= temp;
    pULab->SetSolenoidValve(valveStates);
}
