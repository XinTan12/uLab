#include <QCoreApplication>
#include <QDebug>
// #include <QSocketNotifier>
#include <QTimer>
#include "uLab.h"



int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    ULab controller;

    // 将 controller 对象的 SendMessage 信号，连接到一个用于打印的 Lambda 槽函数
    // 这样，每当 controller 内部 emit SendMessage(msg) 时，下面的 qDebug() 就会被执行
    QObject::connect(&controller, &ULab::SendMessage, [](const QString &msg) {
        qDebug().noquote() << msg;           // 使用 noquote() 可以去掉字符串两边的引号，输出更美观
    });

    if(!controller.InitPort("/dev/tty.usbserial-140"))   // Windows: COMx    // mac: /dev/tty.usbserial-140
    {
        qDebug() << "串口连接失败，程序退出。";
        return 1;
    }


    // *********************************************************************************
    //                                多通道换液测试
    // *********************************************************************************



    // ======================================================================
    // ====================== 1. 用户配置区 (配置controller对象) ===============
    // ======================================================================


    QMap<QString, Setconfig_Pump_in> actionLibrary;

    actionLibrary["PBS_1"] = {"预热PBS洗涤1次",                  // {action_name,
                            1, 1,                              // valve_id_in(连接试剂的切换阀ID), channel_in(对应的通道号),
                            1, 10.0, true,                     // pump_id(蠕动泵ID), speed(期望的抽液速度，uL/s),  isForward(泵的转动方向),
                            2, 1,                              // valve_id_out(连接皿的切换阀ID), channel_out(对应的通道号),
                            200.0 };                           // volume_ul(期望抽取的试剂液体体积，uL) };

    actionLibrary["PBS_2"] = {"PBS冲洗管路",
                              1, 1,
                              1, 10.0, true,
                              2, 2,
                              200.0 };

    actionLibrary["gudingye"] = {"固定液RT固定15min",
                                 1, 2,
                                 1, 10.0, true,
                                 2, 1,
                                 200.0 };

    actionLibrary["tongtouji"] = {"通透剂通透15min",
                                  1, 3,
                                  1, 10.0, true,
                                  2, 1,
                                  200.0 };

    actionLibrary["fengbiye"] = {"封闭液RT封闭1h",
                                 1, 4,
                                 1, 10.0, true,
                                 2, 1,
                                 200.0 };

    actionLibrary["kangti_xishi_1"] = {"抗体稀释液稀释（一抗），4℃过夜",
                                       1, 5,
                                       1, 10.0, true,
                                       2, 1,
                                       200.0 };

    actionLibrary["kangti_xishi_2"] = {"抗体稀释液稀释（二抗），RT避光1h",
                                       1, 6,
                                       1, 10.0, true,
                                       2, 1,
                                       200.0 };



    // ======================================================================



    // ======================================================================
    // =================== 2. 用户实验序列 (调用已配置的动作) ===================
    // ======================================================================

     QTimer::singleShot(1000, &controller, [&]() {

        controller.Pump_in(actionLibrary["PBS_1"]);
        MSleep(1000);

                 //Pump_Peristaltic(蠕动泵的ID, 泵的转动方向, 期望的抽液速度(uL/s), 期望抽取的液体体积(uL))
        controller.Pump_Peristaltic(2, true, 20.0, 500.0);
        qDebug() << "抽液完毕";

        controller.Pump_in(actionLibrary["gudingye"]);
        MSleep(1000);
        controller.Pump_Peristaltic(2, true, 20.0, 500.0);
        qDebug() << "抽液完毕";

        //冲洗管路
        controller.Pump_in(actionLibrary["PBS_2"]);
        MSleep(1000);

        controller.Pump_in(actionLibrary["PBS_1"]);
        MSleep(1000);
        controller.Pump_Peristaltic(2, true, 20.0, 500.0);
        qDebug() << "抽液完毕";

        controller.Pump_in(actionLibrary["tongtouji"]);
        MSleep(1000);
        controller.Pump_Peristaltic(2, true, 20.0, 500.0);
        qDebug() << "抽液完毕";

        controller.Pump_in(actionLibrary["PBS_2"]);
        MSleep(1000);

        controller.Pump_in(actionLibrary["PBS_1"]);
        MSleep(1000);
        controller.Pump_Peristaltic(2, true, 20.0, 500.0);
        qDebug() << "抽液完毕";

        controller.Pump_in(actionLibrary["fengbiye"]);
        MSleep(1000);
        controller.Pump_Peristaltic(2, true, 20.0, 500.0);
        qDebug() << "抽液完毕";

        controller.Pump_in(actionLibrary["kangti_xishi_1"]);
        MSleep(1000);
        controller.Pump_Peristaltic(2, true, 20.0, 500.0);
        qDebug() << "抽液完毕";

        controller.Pump_in(actionLibrary["PBS_2"]);
        MSleep(1000);

        controller.Pump_in(actionLibrary["PBS_1"]);
        MSleep(1000);
        controller.Pump_Peristaltic(2, true, 20.0, 500.0);
        qDebug() << "抽液完毕";

        controller.Pump_in(actionLibrary["kangti_xishi_2"]);
        MSleep(1000);
        controller.Pump_Peristaltic(2, false, 20.0, 500.0);
        qDebug() << "抽液完毕";

        controller.Pump_in(actionLibrary["PBS_2"]);
        MSleep(1000);

        controller.Pump_in(actionLibrary["PBS_1"]);
        MSleep(1000);
        controller.Pump_Peristaltic(2, false, 20.0, 500.0);
        qDebug() << "抽液完毕";

        qDebug() << "\n\n*** 实验执行完毕 ***";

    });

    // *********************************************************************************


    // QObject::connect(&controller, &ULab::EmergencyStopTriggered, [&]() {
    //     qDebug() << "正在清理资源...";
    //     QTimer::singleShot(1000, &a, &QCoreApplication::quit);
    // });

    // 确保在应用退出前关闭端口，可以连接 aboutToQuit 信号
    QObject::connect(&a, &QCoreApplication::aboutToQuit, &controller, [&controller](){
        qDebug() << "应用程序即将退出，关闭串口...";
        controller.ClosePort();
    });

    return a.exec();
}

// #include "main.moc"
