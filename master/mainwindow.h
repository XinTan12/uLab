#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QButtonGroup>
#include <QMainWindow>
#include <QMessageBox>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>

#include "uLab.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void SlotUpdatePos(DEVICE_CODE id, AXIS axis, int pos);

    void SlotUpdatePressure(uint pressure);

    void SlotUpdateFlow(uint flow);

    void ReceiveMessage(QString msg);

    void on_refreshButton_clicked();

    void on_openButton_clicked();

    void on_closeButton_clicked();

    void on_valve1GoButton_clicked();

    void on_valve2GoButton_clicked();

    void on_pump1VelocityNumBox_editingFinished();

    void on_pump2VelocityNumBox_editingFinished();

    void on_pump2StartButton_clicked();

    void on_pump2StopButton_clicked();

    void on_pump1StartButton_clicked();

    void on_pump1StopButton_clicked();

    void on_xAxisLowGotoButton_clicked();

    void on_xAxisLowLeftButton_clicked();

    void on_xAxisLowHomeButton_clicked();

    void on_xAxisLowRightButton_clicked();

    void on_yAxisLowGotoButton_clicked();

    void on_yAxisLowLeftButton_clicked();

    void on_yAxisLowHomeButton_clicked();

    void on_yAxisLowRightButton_clicked();

    void on_zAxisLowGotoButton_clicked();

    void on_zAxisLowLeftButton_clicked();

    void on_zAxisLowHomeButton_clicked();

    void on_zAxisLowRightButton_clicked();

    void on_xAxisLowBox_clicked(bool checked);

    void on_yAxisLowBox_clicked(bool checked);

    void on_zAxisLowBox_clicked(bool checked);

    //    void on_xAxisHighBox_clicked(bool checked);

    void on_xAxisHighGotoButton_clicked();

    void on_xAxisHighLeftButton_clicked();

    void on_xAxisHighHomeButton_clicked();

    void on_xAxisHighRightButton_clicked();

    void on_yAxisHighGotoButton_clicked();

    void on_yAxisHighLeftButton_clicked();

    void on_yAxisHighHomeButton_clicked();

    void on_yAxisHighRightButton_clicked();

    void on_zAxisHighGotoButton_clicked();

    void on_zAxisHighLeftButton_clicked();

    void on_zAxisHighHomeButton_clicked();

    void on_zAxisHighRightButton_clicked();

    void on_xAxisHighBox_clicked(bool checked);

    void on_yAxisHighBox_clicked(bool checked);

    void on_zAxisHighBox_clicked(bool checked);

    void on_airPumpStartButton_clicked();

    void on_pressureSetButton_clicked();

    void on_flowSetButton_clicked();

    void on_FluigentBox_clicked(bool checked);

    void valveButtonGroupClicked(int id);

private:
    Ui::MainWindow *ui;
    ULab *pULab;
    void EnableAction(bool enable);
    bool isAirPumpStarted;
    QButtonGroup valveButtonGroup; //气泵电磁阀按钮组
    uint8_t valveStates;
};
#endif // MAINWINDOW_H
