#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <csignal>
#include "uLab.h"

// 全局指针，用于信号处理函数访问controller
ULab* g_controller = nullptr;

// 信号处理函数
void signalHandler(int signal)
{
    qDebug() << "接收到信号" << signal << "，正在安全停止所有设备...";
    
    if (g_controller) {
        g_controller->StopAllDevices();
        g_controller->ClosePort();
    }
    
    qDebug() << "设备已安全停止，程序退出";
    QCoreApplication::exit(0);
}



int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    ULab controller;
    g_controller = &controller;  // 设置全局指针

    // 注册信号处理函数
    std::signal(SIGINT, signalHandler);   // Ctrl+C
    std::signal(SIGTERM, signalHandler);  // 终止信号
#ifdef SIGBREAK
    std::signal(SIGBREAK, signalHandler); // Windows Ctrl+Break
#endif

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
    //                                免疫荧光测试
    // *********************************************************************************



    // ======================================================================
    // ====================== 1. 用户配置区 (配置controller对象) ===============
    // ======================================================================

    // 配置试剂与第一个切换阀通道的映射关系
    QMap<QString, ReagentConfig> reagentConfigs;
    
    reagentConfigs["PBS"] = {"PBS", 1};                      // PBS洗涤液 -> 第一个切换阀通道1
    reagentConfigs["固定液"] = {"固定液", 2};                 // 固定液 -> 第一个切换阀通道2  
    reagentConfigs["通透剂"] = {"通透剂", 3};                 // 通透剂 -> 第一个切换阀通道3
    reagentConfigs["封闭液"] = {"封闭液", 4};                 // 封闭液 -> 第一个切换阀通道4
    reagentConfigs["一抗稀释液"] = {"一抗稀释液", 5};         // 一抗稀释液 -> 第一个切换阀通道5
    reagentConfigs["二抗稀释液"] = {"二抗稀释液", 6};         // 二抗稀释液 -> 第一个切换阀通道6
    
    // 配置样品与第二个切换阀通道的映射关系
    QMap<QString, SampleConfig> sampleConfigs;
    
    sampleConfigs["废液缸"] = {"废液缸", 1};                    // 废液缸 -> 第二个切换阀通道1
    sampleConfigs["样品1"] = {"样品1", 2};                     // 样品1 -> 第二个切换阀通道2
    sampleConfigs["样品2"] = {"样品2", 3};                    // 样品2 -> 第二个切换阀通道3
    sampleConfigs["样品3"] = {"样品3", 4};                    // 样品3 -> 第二个切换阀通道4
    
    // 应用配置到controller
    controller.SetReagentConfig(reagentConfigs);
    controller.SetSampleConfig(sampleConfigs);


    // ======================================================================



    // ======================================================================
    // =================== 2. 用户实验序列 (调用已配置的动作) ===================
    // ======================================================================

     QTimer::singleShot(1000, &controller, [&]() {

        // 在开始实验前，先进行初始化管路冲洗.
        // 参数:用户更换试剂的间隔时间 (秒)
        controller.InitialWashPipelines(60);

        // AddLiquid参数：试剂名称, 体积(uL), 速度(SLOW/MEDIUM/FAST), 样品名称, 加液抽液的间隔时间(秒)
        // WashPipeline参数：试剂名称, 样品名称
        
        // 1. PBS预热洗涤，样品1
        controller.AddLiquid("PBS", 200.0, MEDIUM, "样品1", 1);

        // 2. 固定液处理，样品1
        controller.AddLiquid("固定液", 200.0, MEDIUM, "样品1", 1);

        // 3. PBS冲洗管路到废液缸
        controller.WashPipeline("PBS", "废液缸");

        // 4. PBS洗涤，样品1
        controller.AddLiquid("PBS", 200.0, MEDIUM, "样品1", 1);

        // 5. 通透剂处理，样品1
        controller.AddLiquid("通透剂", 200.0, MEDIUM, "样品1", 1);

        // 6. PBS冲洗管路到废液缸
        controller.WashPipeline("PBS", "废液缸");

        // 7. PBS洗涤，样品1  
        controller.AddLiquid("PBS", 200.0, MEDIUM, "样品1", 1);

        // 8. 封闭液处理，样品1
        controller.AddLiquid("封闭液", 200.0, MEDIUM, "样品1", 1);

        // 9. 一抗稀释液处理，样品1
        controller.AddLiquid("一抗稀释液", 200.0, MEDIUM, "样品1", 1);

        // 10. PBS冲洗管路到废液缸
        controller.WashPipeline("PBS", "废液缸");

        // 11. PBS洗涤，样品1
        controller.AddLiquid("PBS", 200.0, MEDIUM, "样品1", 1);

        // 12. 二抗稀释液处理，样品1，使用快速，间隔2秒
        controller.AddLiquid("二抗稀释液", 200.0, FAST, "样品1", 2);

        // 13. PBS冲洗管路到废液缸
        controller.WashPipeline("PBS", "废液缸");

        // 14. 最终PBS洗涤，样品1，使用慢速，间隔0.5秒
        controller.AddLiquid("PBS", 200.0, SLOW, "样品1", 0);  // 0秒表示无间隔

        qDebug() << "\n\n*** 实验执行完毕 ***";

    });

    // *********************************************************************************


    // 确保在应用退出前关闭端口，可以连接 aboutToQuit 信号
    QObject::connect(&a, &QCoreApplication::aboutToQuit, &controller, [&controller](){
        qDebug() << "应用程序即将退出，正在安全停止所有设备...";
        controller.StopAllDevices();
        controller.ClosePort();
        qDebug() << "设备已安全停止";
    });

    return a.exec();
}

// #include "main.moc"
