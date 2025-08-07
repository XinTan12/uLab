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

    QObject::connect(&controller, &ULab::AddLiquidCompleted, &a, QCoreApplication::quit);

    if(!controller.InitPort("/dev/tty.usbserial-140"))   // Windows: COMx    // mac: /dev/tty.usbserial-140
    {
        qDebug() << "串口连接失败，程序退出。";
        return 1;
    }

    // *********************************************************************************
    //                                低精度位移台运动控制测试
    // *********************************************************************************

    // 全板遍历运动控制
    QTimer::singleShot(1000, &controller, [&controller]() {
        qDebug() << "开始全板遍历运动...";

        controller.MoveStage(LOW_STAGE_CODE, 15, 15, 1000);      // 指定速度
    });
    //  controller.MoveStage(LOW_STAGE_CODE);                    // 默认速度

    //     参数说明：
    //     200           - X轴速度 (20 * 0.12 = 2.4mm/s)
    //     15            - Y轴速度 (15 * 0.12 = 1.8mm/s)
    //     500           - 每个孔位停留500ms




    //定向移动运动控制
    // QTimer::singleShot(1000, &controller, [&controller]() { // 延时一点启动，确保串口初始化信息打印完毕
    //     qDebug() << "开始定向移动测试...";
    //     controller.MoveStage(LOW_STAGE_CODE,
    //                         QPoint(4, 2),
    //                         AXIS_X,
    //                         true,
    //                          5,
    //                          20,    // X速度
    //                          -1,    // Y速度保持默认
    //                          500);  // 停留时间
    //   });

    /* 从C5孔开始沿X轴正方向移动3孔
     * 参数说明：
     * QPoint(4,2)      - 起始位置（列4=第5列，行2=第3行，C5孔）
     * AXIS_X           - X轴移动（列方向）
     * true             - 正方向（列号增加）
     * 3                - 移动3个孔位
     * 20              - X轴速度 20 * 0.12=2.4mm/s
     * 无Y速度参数       - 保持默认速度
     * 2000             - 每个目标孔停留2秒 */
    // *********************************************************************************



    // 确保在应用退出前关闭端口，可以连接 aboutToQuit 信号
    QObject::connect(&a, &QCoreApplication::aboutToQuit, &controller, [&controller](){
        qDebug() << "应用程序即将退出，关闭串口...";
        controller.ClosePort();
    });

    return a.exec();
}

// #include "main.moc"
