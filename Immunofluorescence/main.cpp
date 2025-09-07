#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <QThread>
#include <csignal>
#include <QSocketNotifier>
#include <QTextStream>
#include <cstdio>
#include <iostream>
#include <string>
#ifdef Q_OS_WIN
#include <conio.h>
#endif
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
    //QCoreApplication::exit(0);  //只会退出Qt的事件循环，但程序的 main()后续代码还会继续执行。
    std::exit(0); //彻底终止程序
}



int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    ULab controller;
    g_controller = &controller;  // 设置全局指针

    // 注册信号处理函数
    std::signal(SIGINT, signalHandler);   // Ctrl+C，中断信号(2),在控制台按Ctrl+C时发送
    std::signal(SIGTERM, signalHandler);  // 终止信号(15),Qt Creator点击"停止"按钮时发送的信号
    std::signal(SIGABRT, signalHandler);  // 异常终止(6)，程序调用abort()函数或断言失败时触发
#ifdef SIGBREAK
    std::signal(SIGBREAK, signalHandler); // Windows Ctrl+Break(21)
#endif
#ifdef Q_OS_WIN
    // Windows特有信号
    std::signal(SIGFPE, signalHandler);   // 浮点异常(8)，除零或其他数学错误时触发
    std::signal(SIGILL, signalHandler);   // 非法指令(4)
    std::signal(SIGSEGV, signalHandler);  // 段错误（内存访问错误）(11),程序访问无效内存地址时自动触发
#endif

    // 将 controller 对象的 SendMessage 信号，连接到一个用于打印的 Lambda 槽函数
    // 这样，每当 controller 内部 emit SendMessage(msg) 时，下面的 qDebug() 就会被执行
    QObject::connect(&controller, &ULab::SendMessage, [](const QString &msg) {
        qDebug().noquote() << msg;           // 使用 noquote() 可以去掉字符串两边的引号，输出更美观
    });

    QObject::connect(&controller, &ULab::UserInputReceived, &controller, &ULab::onUserInputReceived, Qt::UniqueConnection);

    // 修改输入处理机制 - 适配Qt Creator环境
    QSocketNotifier* stdinNotifier = nullptr;
    QTextStream* inputStream = nullptr;
    
#ifdef Q_OS_WIN
    // Windows下使用线程方式处理输入
    QTimer inputTimer;
    inputTimer.setInterval(50); // 50ms检查一次，提高响应性
    inputStream = new QTextStream(stdin);
    
    // 使用QThread在后台监听stdin，避免阻塞主线程
    QObject::connect(&inputTimer, &QTimer::timeout, [&controller]() {
        // 使用标准C函数检查stdin
        if (_kbhit()) { // Windows特有的函数，检查键盘是否有输入
            std::string stdLine;
            std::getline(std::cin, stdLine);
            QString line = QString::fromStdString(stdLine).trimmed();
            if (!line.isEmpty()) {
                qDebug() << "接收到用户输入:" << line;
                emit controller.UserInputReceived(line);
            }
        }
    });
    inputTimer.start();
#else
    // Unix/Linux下使用QSocketNotifier
    stdinNotifier = new QSocketNotifier(fileno(stdin), QSocketNotifier::Read);
    inputStream = new QTextStream(stdin);
    
    QObject::connect(stdinNotifier, &QSocketNotifier::activated, [&controller, inputStream](int) {
        QString line = inputStream->readLine().trimmed();
        if (!line.isEmpty()) {
            qDebug() << "接收到用户输入:" << line;
            emit controller.UserInputReceived(line);
        }
    });
#endif

    if(!controller.InitPort("COM5"))   // Windows: COMx    // mac: /dev/tty.usbserial-140
    {
        qDebug() << "串口连接失败，程序退出。";
        return 1;
    }


    // *********************************************************************************
    //                                免疫荧光实验
    // *********************************************************************************



    // ======================================================================
    // ====================== 1. 用户配置区 (配置controller对象) ===============
    // ======================================================================

    // 配置试剂与第一个切换阀通道的映射关系
    QMap<QString, ReagentConfig> reagentConfigs;
    
    reagentConfigs["PBS"] = {"PBS", 1};                      // PBS洗涤液 -> 第一个切换阀通道1
    reagentConfigs["固定液"] = {"固定液", 2};                 // 固定液 -> 第一个切换阀通道2
    //reagentConfigs["通透剂"] = {"通透剂", 3};                 // 通透剂 -> 第一个切换阀通道3
    //reagentConfigs["封闭液"] = {"封闭液", 4};                 // 封闭液 -> 第一个切换阀通道4
    //reagentConfigs["一抗稀释液"] = {"一抗稀释液", 5};         // 一抗稀释液 -> 第一个切换阀通道5
    //reagentConfigs["二抗稀释液"] = {"二抗稀释液", 6};         // 二抗稀释液 -> 第一个切换阀通道6
    
    // 配置样品与第二个切换阀通道的映射关系
    QMap<QString, SampleConfig> sampleConfigs;
    
    sampleConfigs["废液缸"] = {"废液缸", 1};                    // 废液缸 -> 第二个切换阀通道1
    sampleConfigs["样品1"] = {"样品1", 2};                     // 样品1 -> 第二个切换阀通道2
    //sampleConfigs["样品2"] = {"样品2", 3};                    // 样品2 -> 第二个切换阀通道3
    //sampleConfigs["样品3"] = {"样品3", 4};                    // 样品3 -> 第二个切换阀通道4
    
    // 应用配置到controller
    controller.SetReagentConfig(reagentConfigs);
    controller.SetSampleConfig(sampleConfigs);


    qDebug() << "\n\n=======================================================";
    qDebug() << "*** 免疫荧光实验即将开始 ***";
    qDebug() << "实验流程期间可随时在在下方 Terminal 输出框中:";
    qDebug() << "- 输入 'q' 或 'quit' 或 ‘exit’ 退出程序";
    qDebug() << "=======================================================";

    // ======================================================================



    // ======================================================================
    // =================== 2. 用户实验序列 (调用已配置的动作) ===================
    // ======================================================================

     QTimer::singleShot(1000, &controller, [&]() {

        // 在开始实验前，先进行初始化管路冲洗
        controller.InitialWashPipelines();

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
    QObject::connect(&a, &QCoreApplication::aboutToQuit, [&controller, inputStream, stdinNotifier](){
        qDebug() << "\n应用程序即将退出，正在安全停止所有设备...";
        
        controller.StopAllDevices();
        controller.ClosePort();
        
        // 清理输入流资源
        if (inputStream) {
            delete inputStream;
        }
        if (stdinNotifier) {
            delete stdinNotifier;
        }
        
        qDebug() << "\n设备已安全停止";
    });

    return a.exec();
}

// #include "main.moc"
