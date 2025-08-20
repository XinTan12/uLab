#include "uLab.h"
#include <QCoreApplication>
#include <QtMath>
#include <QDebug>
#include <QTextStream>
#include <QThread>

QString getWellName(int row, int col)
{
    if (row < 0 || row > 7 || col < 0 || col > 11) {
        return "Unknown"; // 边界检查
    }
    // 'A' 的ASCII码加上行号 (0-7)，得到 'A'-'H'
    QChar rowChar = QChar('A' + row);
    // 列号 (0-11) + 1，得到 1-12
    return QString("%1%2").arg(rowChar).arg(col + 1);
}

ULab::ULab(QObject *parent) : QObject(parent)
{
    pPort = new QSerialPort(this);
    pPortTimer = new QTimer(this);
    pParseTimer = new QTimer(this);
    pReadTimer = new QTimer(this);
    pGetFlowTimer = new QTimer(this);
    m_pumpInterval = 1000; // 默认间隔1秒
    connect(pPortTimer, &QTimer::timeout, this, &ULab::RefreshPort);
    connect(pParseTimer, &QTimer::timeout, this, &ULab::ParsePort);
    //    pCRC = new CRC();
}

ULab::~ULab()
{
    // 程序退出时停止所有设备
    StopAllDevices();
    
    ClosePort();
    delete pPortTimer;
    delete pParseTimer;
    delete pReadTimer;
    delete pGetFlowTimer;
    delete pPort;
    //    delete pCRC;
}

bool ULab::InitPort(QString portName)
{
    pPort->setPortName(portName);
    pPort->setBaudRate(QSerialPort::Baud115200);
    pPort->setDataBits(QSerialPort::Data8);
    pPort->setStopBits(QSerialPort::OneStop);
    if (pPort->open(QIODevice::ReadWrite))
    {
        pPortTimer->start(CMD_INTERVAL);
        pParseTimer->start(PARSE_INTERVAL);
        pReadTimer->start(READ_INTERVAL);
        pGetFlowTimer->start(FLOW_INTERVAL);
        emit SendMessage("Succeed in connecting " + portName);
        return true;
    }
    else
    {
        emit SendMessage("Failed to connect to" + portName);
        return false;
    }
}

void ULab::ClosePort()
{
    pPortTimer->stop();
    pParseTimer->stop();
    pReadTimer->stop();
    if (pPort->isOpen())
        pPort->close();
    emit SendMessage("Disconnect from " + portName);
}

void ULab::Rotate(bool start, bool direction, uint8_t id)
{
    wrtCmdList.append(GenCMD(0x0A, id, !direction ? 0x01 : 0x00, start ? 0x01 : 0x02));
    emit SendMessage(QString("Peristaltic pump (ID:%1) ").arg(id) + (start ? (QString("start to rotate in ") +
                                               (direction ? "normal" : "reverse") + " direction") : " stop rotating"));
}

void ULab::SetSpeed(uint16_t speed, uint8_t id)
{
    wrtCmdList.append(GenCMD(0x09, id, speed >> 8, speed & 0xff));
    emit SendMessage("Set peristaltic pump speed to " + QString::number(speed));
}

void ULab::GotoChannel(uint8_t addr, uint8_t channel, uint8_t id)
{
    wrtCmdList.append(GenCMD(0x08, id, channel, addr));
    emit SendMessage("Valve (addr:" + QString::number(addr) + ") go to channel No." + QString::number(channel));
}

void ULab::Home(AXIS axis, DEVICE_CODE id)
{
    wrtCmdList.append(GenCMD(axis, id, 0x00, 0x00));
    emit SendMessage(GetAxisName(axis) + " of low-precision table go back to home");
    m_currentPos[id] = QPoint(0,0);
}

void ULab::Goto(AXIS axis, uint16_t pos, DEVICE_CODE id)
{
    //    if (pos >= (1<<17)-1)
    //    {
    //        emit SendMessage(GetAxisName(axis) + " of low-precision table's destination is out of range!");
    //        return;
    //    }
    //    uint16_t pos_ = pos / 2;
    wrtCmdList.append(GenCMD(1+axis, id, pos >> 8, pos & 0xff));
    emit SendMessage(GetAxisName(axis) + " of " + (id == LOW_STAGE_CODE ? "low-precision" : "high-precision") + "table go to " + QString::number(pos));
}

void ULab::SetSpeedStage(AXIS axis, uint16_t speed, DEVICE_CODE id)
{
    wrtCmdList.append(GenCMD(2+axis, id, speed >> 8, speed & 0xff));
}

void ULab::SetTime(AXIS axis, uint16_t time, DEVICE_CODE id)
{
    wrtCmdList.append(GenCMD(3+axis, id, time >> 8, time & 0xff));
}

void ULab::Go(AXIS axis, bool direction, DEVICE_CODE id)
{
    wrtCmdList.append(GenCMD(4+axis, id, direction ? 0x01: 0x00, 0x00));
}

void ULab::Enable(AXIS axis, bool enable, DEVICE_CODE id)
{
    wrtCmdList.append(GenCMD(5+axis, id, enable ? 0x00: 0x01, 0x00));
}

void ULab::GetPos(AXIS axis, DEVICE_CODE id)
{
    wrtCmdList.append(GenCMD(7+axis, id, 1+axis, 0x00));
}

void ULab::SetAxisEnable(AXIS axis, bool enable, DEVICE_CODE code)
{
    Enable(axis, enable, code);
    if (code == LOW_STAGE_CODE)
    {
        if (enable)
        {
            switch (axis)
            {
            case AXIS_X: connect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosLowX);break;
            case AXIS_Y: connect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosLowY);break;
            case AXIS_Z: connect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosLowZ);break;
            default:;
            }
        }
        else
        {
            switch (axis)
            {
            case AXIS_X: disconnect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosLowX);break;
            case AXIS_Y: disconnect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosLowY);break;
            case AXIS_Z: disconnect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosLowZ);break;
            default:;
            }
        }
    }
    else if (code == HIGH_STAGE_CODE)
    {
        if (enable)
        {
            switch (axis)
            {
            case AXIS_X: connect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosHighX);break;
            case AXIS_Y: connect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosHighY);break;
            case AXIS_Z: connect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosHighZ);break;
            default:;
            }
        }
        else
        {
            switch (axis)
            {
            case AXIS_X: disconnect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosHighX);break;
            case AXIS_Y: disconnect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosHighY);break;
            case AXIS_Z: disconnect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosHighZ);break;
            default:;
            }
        }
    }
}

void ULab::SetFluigentEnable(bool enable)
{
    if (enable)
    {
        connect(pGetFlowTimer, &QTimer::timeout, this, &ULab::GetPresAndFlow);
    }
    else
    {
        disconnect(pGetFlowTimer, &QTimer::timeout, this, &ULab::GetPresAndFlow);
    }
}

void ULab::StartPump(uint16_t speed)
{
    wrtCmdList.append(GenCMD(0x31, PUMP_CODE, speed >> 8, speed & 0xff));
}

void ULab::StopPump()
{
    wrtCmdList.append(GenCMD(0x32, PUMP_CODE, 0x00, 0x00));
}

void ULab::SetPressure(uint16_t pressure)
{
    QByteArray cmd = GenCMD(0x20, PUMP_CODE, pressure >> 8, pressure & 0xff);
    for (int i = 0; i < 3; ++i)
        wrtCmdList.append(cmd);
}

void ULab::SetFlow(uint16_t flow)
{
    QByteArray cmd = GenCMD(0x21, PUMP_CODE, flow >> 8, flow & 0xff);
    for (int i = 0; i < 3; ++i)
        wrtCmdList.append(cmd);
}

void ULab::GetPressure()
{
    wrtCmdList.append(GenCMD(0x24, PUMP_CODE, 0x00, 0x00));
}

void ULab::GetFlow()
{
    wrtCmdList.append(GenCMD(0x23, PUMP_CODE, 0x00, 0x00));
}

void ULab::PeristalticPumpRotate(bool start)
{
    wrtCmdList.append(GenCMD(0x52, PUMP_CODE, start ? 0x01 : 0x00, 0x00));
}

void ULab::PeristalticPumpSetSpeed(uint16_t speed)
{
    wrtCmdList.append(GenCMD(0x51, PUMP_CODE, speed >> 8 , speed & 0xff));
}

void ULab::SetSolenoidValve(uint8_t valves)
{
    wrtCmdList.append(GenCMD(0x41, PUMP_CODE, valves , 0x00));
}

void ULab::RefreshPort()
{
    if (!wrtCmdList.isEmpty())
    {
        pPort->write(wrtCmdList.takeFirst());
    }
}

void ULab::ParsePort()
{
    readBuffer += pPort->readAll();

    for (int index = 0; ; index++)
    {
        if (index + 7 >= readBuffer.length())
        {
            break;
        }
        if (readBuffer.at(index) == (char)0xFE && readBuffer.at(index + 7) == (char)0xFF)
        {
            QByteArray cmd = readBuffer.mid(index, 8);
            readBuffer.remove(index--, 8);
            if (!CheckCMD(cmd))
            {
                continue;
            }
            switch (cmd.at(2))
            {
            case PIPET_CODE:break;
            case LOW_STAGE_CODE:                //低精度位移台回复指令
            case HIGH_STAGE_CODE:               //高精度位移台回复指令
            {
                uint pos = ((uint)(uint8_t)cmd.at(3) << 8) + cmd.at(4);
                emit UpdatePos((DEVICE_CODE)cmd.at(2), (AXIS)(cmd.at(1)-7), pos);
                break;
            }
            case PUMP_CODE:                     //气泵回复指令
            {
                if (cmd.at(1) == 0x24)
                {
                    uint pressure = ((uint)(uint8_t)cmd.at(3) << 8) + cmd.at(4);
                    emit UpdatePressure(pressure);
                }
                else if (cmd.at(1) == 0x23)
                {
                    uint flow = ((uint)(uint8_t)cmd.at(3) << 8) + cmd.at(4);
                    emit UpdateFlow(flow);
                }
                break;
            }
            default:;
            }
        }
    }
}

void ULab::GetPresAndFlow()
{
    GetPressure();
    MSleep(FLOW_INTERVAL / 2);
    GetFlow();
}

QByteArray ULab::GenCMD(uint8_t code, uint8_t id, uint8_t contentH, uint8_t contentL)
{
    QByteArray cmd = QByteArray::fromHex("FE").append(code).append(id).append(contentH).append(contentL);
    cmd.append(CRCMDBS_GetValue(cmd)).append(0xFF);
    return cmd;
}

bool ULab::CheckCMD(QByteArray cmd)
{
    if (cmd.length() != 8 || cmd.at(0) != (char)0xFE || cmd.at(7) != (char)0xFF)
    {
        return false;
    }
    if (CRCMDBS_GetValue(cmd.left(5)) != cmd.mid(5, 2))
    {
        return false;
    }
    return true;
}

//CRC高位字节值表
const unsigned char CRC_High[] = {
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
};
//CRC 低位字节值表
const unsigned char CRC_Low[]= {
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06,
    0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD,
    0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
    0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A,
    0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4,
    0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
    0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3,
    0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,
    0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
    0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29,
    0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED,
    0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
    0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60,
    0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67,
    0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
    0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,
    0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E,
    0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
    0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71,
    0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92,
    0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
    0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B,
    0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B,
    0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
    0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42,
    0x43, 0x83, 0x41, 0x81, 0x80, 0x40
};

QByteArray CRCMDBS_GetValue(QByteArray msg)
{
    unsigned char m_CRC_High=0xFF; 	//高CRC 字节初始化
    unsigned char m_CRC_Low=0xFF; 	//低CRC 字节初始化
    unsigned char uIndex; 			//CRC 循环中的索引
    for (int index = 0; index < msg.size(); ++index)
    {
        uIndex = m_CRC_Low ^ msg.at(index);
        m_CRC_Low = m_CRC_High ^ CRC_High[uIndex];
        m_CRC_High = CRC_Low[uIndex];
    }
    return QByteArray().append(m_CRC_High).append(m_CRC_Low);
    //    return (m_CRC_High<<8|m_CRC_Low);
}

QString GetAxisName(AXIS axis)
{
    switch(axis)
    {
    case AXIS_X: return "AXIS X";
    case AXIS_Y: return "AXIS Y";
    case AXIS_Z: return "AXIS Z";
    default: return "";
    }
}

// 延时函数，阻塞当前函数执行，但仍能处理Qt事件，串口数据接收、定时器等可继续工作
void MSleep(uint msec)
{
    QEventLoop loop;
    QTimer::singleShot(msec, &loop, SLOT(quit()));
    loop.exec();
}

// 可中断的延时函数
void MSleepInterruptible(uint msec)
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    // 每100ms检查一次是否应该停止
    const uint checkInterval = 100;
    uint elapsed = 0;
    
    while (elapsed < msec) {
        int sleepTime = qMin(checkInterval, static_cast<uint>(msec - elapsed));
        timer.start(sleepTime);
        loop.exec();
        elapsed += sleepTime;
        
        // 检查全局停止标志
        if (QCoreApplication::instance() && 
            QCoreApplication::instance()->property("shouldStop").toBool()) {
            qDebug() << "检测到停止信号，中断延时操作";
            break;
        }
    }
}

void ULab::SendData(const QByteArray &data)
{
    if (pPort && pPort->isOpen()) {
        pPort->write(data);
        pPort->flush();
    }
}

// ******************************************************************************

// ******************************* 多通道换液流程 *********************************

void ULab::SetReagentConfig(const QMap<QString, ReagentConfig>& config)
{
    m_reagentConfigs = config;
    emit SendMessage(QString("试剂配置已更新，共配置 %1 种试剂").arg(config.size()));
}

void ULab::SetSampleConfig(const QMap<QString, SampleConfig>& config)
{
    m_sampleConfigs = config;
    emit SendMessage(QString("样品配置已更新，共配置 %1 个样品").arg(config.size()));
}

void ULab::AddLiquid(const QString& reagent_name, double volume_ul, FluidSpeed speed, const QString& sample_name, uint delay_sec)
{
    emit SendMessage("\n\n");
    emit SendMessage(QString("=").repeated(60));

    // 检查试剂是否已配置
    if (!m_reagentConfigs.contains(reagent_name)) {
        emit SendMessage(QString("\n错误：试剂: [%1] 未在配置中找到").arg(reagent_name));
        return;
    }
    
    // 检查样品是否已配置
    if (!m_sampleConfigs.contains(sample_name)) {
        emit SendMessage(QString("\n错误：'%1' 未在配置中找到").arg(sample_name));
        return;
    }

    // 获取试剂和样品配置
    ReagentConfig reagent = m_reagentConfigs[reagent_name];
    SampleConfig sample = m_sampleConfigs[sample_name];
    
    // 根据速度枚举转换为实际流速值 (uL/s)
    double flow_speed = 0.0;
    switch (speed) {
        case SLOW:   flow_speed = 20.0;  break;  // 慢速：20 uL/s
        case MEDIUM: flow_speed = 50.0;  break;  // 中速：50 uL/s
        case FAST:   flow_speed = 100.0; break;  // 快速：100 uL/s
    }

    emit SendMessage(QString("\n[加液操作]: %1uL %2 --> %3 (%4速)")
                         .arg(QString::number(volume_ul),
                              reagent_name,
                              sample_name,
                              speed == SLOW ? "慢" : (speed == MEDIUM ? "中" : "快")));


    // 切换第一个切换阀到试剂通道
    emit SendMessage(QString("\n  > 切换[试剂阀] 到 [通道%1]").arg(reagent.valve_channel));
    GotoChannel(REAGENT_VALVE_ADDR, reagent.valve_channel, 0x01);
    
    MSleep(VALVE_SWITCH_DELAY_MS);
    
    // 切换第二个切换阀到样品通道
    emit SendMessage(QString("\n  > 切换[样品阀] 到 [通道%1]").arg(sample.valve_channel));
    GotoChannel(SAMPLE_VALVE_ADDR, sample.valve_channel, 0x01);
    
    MSleep(VALVE_SWITCH_DELAY_MS);

    // 执行加液操作
    
    if (flow_speed <= 0) {
        emit SendMessage("\n  > 错误：流速输入有误，无法计算时长。");
        return;
    }
    
    uint duration_ms = static_cast<uint>((volume_ul + DEAD_VOLUME) / flow_speed * 1000.0);

    // 启动蠕动泵加液
    SetSpeed(flow_speed, PUMP_IN_ID);
    Rotate(true, true, PUMP_IN_ID);
    emit SendMessage(QString("\n  > 开始加液"));

    MSleepInterruptible(duration_ms);
    Rotate(false, true, PUMP_IN_ID);
    emit SendMessage(QString("\n  > 加液完成"));
    
    // 用户设置的间隔时间(转换秒为毫秒)
    uint interval_ms = delay_sec * 1000;
    if (interval_ms > 0) {
        emit SendMessage(QString("\n  > 等待 %1 秒...").arg(delay_sec));
        MSleep(interval_ms);
    }

    // 启动蠕动泵抽液
    SetSpeed(flow_speed, PUMP_OUT_ID);
    Rotate(true, false, PUMP_OUT_ID);
    emit SendMessage(QString("\n  > 开始抽液"));

    MSleepInterruptible(duration_ms);
    Rotate(false, false, PUMP_OUT_ID);
    emit SendMessage(QString("\n  > 抽液完成"));

    emit SendMessage(QString("\n  > '%1' 操作完成.").arg(reagent_name));
}

void ULab::WashPipeline(const QString& reagent_name, const QString& sample_name)
{
    // 检查试剂是否已配置
    if (!m_reagentConfigs.contains(reagent_name)) {
        emit SendMessage(QString("\n错误：试剂: [%1] 未在配置中找到").arg(reagent_name));
        return;
    }

    // 检查样品是否已配置
    if (!m_sampleConfigs.contains(sample_name)) {
        emit SendMessage(QString("\n错误：[%1] 未在配置中找到").arg(sample_name));
        return;
    }

    // 获取试剂和样品配置
    ReagentConfig reagent = m_reagentConfigs[reagent_name];
    SampleConfig sample = m_sampleConfigs[sample_name];

    emit SendMessage(QString("\n[冲洗管路]: 使用 [%1] 冲洗到 [%2]")
                         .arg(reagent_name, sample_name));


    // 切换第一个切换阀到试剂通道
    GotoChannel(REAGENT_VALVE_ADDR, reagent.valve_channel, 0x01);

    MSleep(VALVE_SWITCH_DELAY_MS);
    
    // 切换第二个切换阀到废液缸通道
    GotoChannel(SAMPLE_VALVE_ADDR, sample.valve_channel, 0x01);

    MSleep(VALVE_SWITCH_DELAY_MS);

    // 执行冲洗操作 - 使用固定的速度和时间
    emit SendMessage(QString("\n  > 开始冲洗管路"));

    // 启动蠕动泵进行冲洗
    SetSpeed(WASH_SPEED, PUMP_IN_ID);
    Rotate(true, true, PUMP_IN_ID);

    MSleepInterruptible(WASH_DURATION_SEC * 1000);  // 使用可中断的延时

    Rotate(false, true, PUMP_IN_ID);
    emit SendMessage(QString("\n  > 管路冲洗完成"));
}

void ULab::InitialWashPipelines()
{
    emit SendMessage(QString("\n\n*** 开始初始化管路冲洗 ***"));
    emit SendMessage(QString("\n注意：初始时所有通道都连接[PBS]，冲洗完成后请更换为[实际试剂]"));
    
    // 获取试剂通道和样品通道列表，并按通道号排序
    QList<uint8_t> reagentChannels;
    QList<uint8_t> sampleChannels;
    
    for (auto it = m_reagentConfigs.begin(); it != m_reagentConfigs.end(); ++it) {
        reagentChannels.append(it.value().valve_channel);
    }
    
    for (auto it = m_sampleConfigs.begin(); it != m_sampleConfigs.end(); ++it) {
        sampleChannels.append(it.value().valve_channel);
    }
    
    // 按通道号排序
    std::sort(reagentChannels.begin(), reagentChannels.end());
    std::sort(sampleChannels.begin(), sampleChannels.end());
    
    // 构建通道号字符串用于显示
    QStringList reagentChStrList, sampleChStrList;
    for (uint8_t ch : reagentChannels) {
        reagentChStrList << QString::number(ch);
    }
    for (uint8_t ch : sampleChannels) {
        sampleChStrList << QString::number(ch);
    }
    
    emit SendMessage(QString("试剂通道: %1个 [%2]").arg(reagentChannels.size()).arg(reagentChStrList.join(",")));
    emit SendMessage(QString("样品通道: %1个 [%2]").arg(sampleChannels.size()).arg(sampleChStrList.join(",")));
    
    // 找出废液缸通道（通道号最小的样品通道）
    uint8_t wasteChannel = sampleChannels.isEmpty() ? 1 : sampleChannels.first();
    emit SendMessage(QString("废液缸所在通道: %1 (默认为通道号最小的样品通道)").arg(wasteChannel));
    
    int reagentCount = reagentChannels.size();
    int sampleCount = sampleChannels.size();
    
    emit SendMessage(QString("\n开始按策略进行管路冲洗:"));
    
    if (reagentCount < sampleCount) {
        // 情况1: 试剂数量 < 样品数量
        emit SendMessage(QString("=== 策略1: 试剂数量(%1) < 样品数量(%2) ===").arg(reagentCount).arg(sampleCount));
        
        // 首先按对应关系冲洗
        for (int i = 0; i < reagentCount; i++) {
            uint8_t reagentCh = reagentChannels[i];
            uint8_t sampleCh = sampleChannels[i];
            
            emit SendMessage(QString("\n[冲洗] 试剂通道%1 -> 样品通道%2").arg(reagentCh).arg(sampleCh));
            performWash(reagentCh, sampleCh, wasteChannel);
        }
        
        // 剩余样品通道都用最小的试剂通道冲洗
        uint8_t minReagentCh = reagentChannels.first();
        for (int i = reagentCount; i < sampleCount; i++) {
            uint8_t sampleCh = sampleChannels[i];
            
            emit SendMessage(QString("\n[冲洗] 试剂通道%1 -> 样品通道%2 (剩余样品通道)").arg(minReagentCh).arg(sampleCh));
            performWash(minReagentCh, sampleCh, wasteChannel);
        }
        
    } else if (reagentCount == sampleCount) {
        // 情况2: 试剂数量 = 样品数量
        emit SendMessage(QString("=== 策略2: 试剂数量(%1) = 样品数量(%2) ===").arg(reagentCount).arg(sampleCount));
        
        for (int i = 0; i < reagentCount; i++) {
            uint8_t reagentCh = reagentChannels[i];
            uint8_t sampleCh = sampleChannels[i];
            
            emit SendMessage(QString("\n[冲洗] 试剂通道%1 -> 样品通道%2").arg(reagentCh).arg(sampleCh));
            performWash(reagentCh, sampleCh, wasteChannel);
        }
        
    } else {
        // 情况3: 试剂数量 > 样品数量
        emit SendMessage(QString("=== 策略3: 试剂数量(%1) > 样品数量(%2) ===").arg(reagentCount).arg(sampleCount));
        
        // 首先按对应关系冲洗
        for (int i = 0; i < sampleCount; i++) {
            uint8_t reagentCh = reagentChannels[i];
            uint8_t sampleCh = sampleChannels[i];
            
            emit SendMessage(QString("\n[冲洗] 试剂通道%1 -> 样品通道%2").arg(reagentCh).arg(sampleCh));
            performWash(reagentCh, sampleCh, wasteChannel);
        }
        
        // 剩余试剂通道都连接废液缸冲洗
        for (int i = sampleCount; i < reagentCount; i++) {
            uint8_t reagentCh = reagentChannels[i];
            
            emit SendMessage(QString("\n[冲洗] 试剂通道%1 -> 废液缸通道%2 (剩余试剂通道)").arg(reagentCh).arg(wasteChannel));
            performWash(reagentCh, wasteChannel, wasteChannel);
        }
    }
    
    emit SendMessage(QString("\n*** 初始化管路冲洗完成 ***"));
    emit SendMessage(QString("请现在更换为各通道的[实际试剂]"));
    
    // 等待用户输入确认
    WaitForUserInput("请完成试剂更换后，在下方 Terminal 输出框中:");
    
    emit SendMessage(QString("继续执行实验流程...\n"));
}

void ULab::performWash(uint8_t reagentChannel, uint8_t sampleChannel, uint8_t wasteChannel)
{
    emit SendMessage(QString("\n  > 切换第一个阀到[试剂通道%1]").arg(reagentChannel));
    // 切换到试剂通道
    GotoChannel(REAGENT_VALVE_ADDR, reagentChannel, 0x01);

    MSleep(VALVE_SWITCH_DELAY_MS);
    
    emit SendMessage(QString("\n  > 第一个阀切换完成，开始切换第二个阀到[样品通道%1]").arg(sampleChannel));
    // 切换到样品通道
    GotoChannel(SAMPLE_VALVE_ADDR, sampleChannel, 0x01);
    
    //MSleep(VALVE_SWITCH_DELAY_MS);
    emit SendMessage(QString("\n  > 第二个阀切换完成"));
    
    // 启动加液泵进行冲洗
    SetSpeed(WASH_SPEED, PUMP_IN_ID);
    Rotate(true, true, PUMP_IN_ID);
    MSleepInterruptible(WASH_DURATION_SEC * 1000);  // 使用可中断的延时
    Rotate(false, true, PUMP_IN_ID);
    
    emit SendMessage(QString("\n  > 加液完成"));
    
    // 判断是否需要抽液：如果样品通道不是废液缸，则需要抽液
    if (sampleChannel != wasteChannel) {
        emit SendMessage(QString("\n  > 开始抽液（非废液缸）"));
        
        // 启动抽液泵
        SetSpeed(WASH_SPEED, PUMP_OUT_ID);
        Rotate(true, true, PUMP_OUT_ID);  // 抽液方向
        MSleepInterruptible(WASH_DURATION_SEC * 1000);  // 使用可中断的延时
        Rotate(false, true, PUMP_OUT_ID);
        
        emit SendMessage(QString("\n  > 抽液完成"));
    } else {
        emit SendMessage(QString("\n  > 跳过抽液（废液缸）"));
    }
    
    emit SendMessage(QString("\n  > 冲洗完成"));
    MSleep(500); // 短暂间隔
}

void ULab::StopAllDevices()
{
    emit SendMessage(QString("\n正在停止所有设备..."));
    
    // 设置停止标志
    m_shouldStop.storeRelaxed(1);
    QCoreApplication::instance()->setProperty("shouldStop", true);
    
    // 立即停止所有蠕动泵
    Rotate(false, true, PUMP_IN_ID);   // 停止加液泵

    MSleep(100);

    Rotate(false, true, PUMP_OUT_ID);  // 停止抽液泵

    // 立即发送命令
    // if (pPort && pPort->isOpen()) {
    //     while (!wrtCmdList.isEmpty()) {
    //         QByteArray cmd = wrtCmdList.takeFirst();
    //         pPort->write(cmd);
    //         pPort->flush();
    //         MSleep(10);  // 短暂间隔确保命令发送
    //     }
    // }

    MSleep(100);  // 这个间隔决定能否让蠕动泵停止转动！！！
    
    // 清空剩余的待发送命令队列
    wrtCmdList.clear();
    
    // 确保串口数据全部发送完成
    // if (pPort && pPort->isOpen()) {
    //     pPort->flush();
    //     pPort->waitForBytesWritten(1000);  // 等待最多1秒
    // }
    
    emit SendMessage(QString("\n所有设备已停止"));
}

void ULab::WaitForUserInput(const QString& message)
{
    QString separator = QString("=").repeated(60);
    emit SendMessage(QString("\n") + separator);
    emit SendMessage(message);
    emit SendMessage(QString("  - 输入 'c' 或 'continue' 继续执行"));
    emit SendMessage(QString("  - 输入 'q' 或 'quit' 或 ‘exit’ 退出程序"));
    emit SendMessage(QString("然后按回车键确认"));
    //emit SendMessage(QString("调试：正在等待用户输入，当前状态: m_waitingForInput=%1").arg(m_waitingForInput));
    emit SendMessage(separator);
    emit SendMessage(QString("\n - 请输入："));
    
    m_waitingForInput = true;
    m_userInput.clear();
    
    // 连接信号槽以处理用户输入
    //connect(this, &ULab::UserInputReceived, this, &ULab::onUserInputReceived, Qt::UniqueConnection);
    
    // 创建事件循环等待用户输入
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(false);
    timer.setInterval(1000); // 每秒检查一次
    
    connect(&timer, &QTimer::timeout, [&]() {
        if (!m_waitingForInput) {
            loop.quit();
        }
        // 检查停止标志
        if (m_shouldStop.loadRelaxed()) {
            emit SendMessage(QString("检测到停止信号，取消等待用户输入"));
            m_waitingForInput = false;
            loop.quit();
        }
    });
    
    timer.start();
    loop.exec();
    timer.stop();
    
    // 断开连接
    //disconnect(this, &ULab::UserInputReceived, this, &ULab::onUserInputReceived);
}

void ULab::onUserInputReceived(QString input)
{
    //emit SendMessage(QString("调试：接收到输入信息 '%1'").arg(input));

    QString cleanInput = input.trimmed().toLower();
    //emit SendMessage(QString("调试：处理清理后的输入 '%1'").arg(cleanInput));

    // quit命令全局有效，无论是否在等待输入状态
    if (cleanInput == "quit" || cleanInput == "q" || cleanInput == "exit") {
        emit SendMessage(QString("收到退出指令，正在停止所有设备并退出程序..."));

        // 停止所有设备
        StopAllDevices();

        // 如果正在等待用户输入，结束等待
        if (m_waitingForInput) {
            m_waitingForInput = false;
        }

        //QCoreApplication::exit(0);  //只会退出Qt的事件循环，但程序的 main()后续代码还会继续执行。
        std::exit(0); //彻底终止程序
        return;
    }

    // 只有在等待输入状态下才处理continue命令
    if (!m_waitingForInput) {
        //emit SendMessage(QString("调试：当前不在等待输入状态，除quit外的命令被忽略"));
        return;
    }

    if (cleanInput == "continue" || cleanInput == "c") {
        emit SendMessage(QString("收到继续指令，程序继续执行"));
        m_waitingForInput = false;
    } else {
        emit SendMessage(QString("无效输入 '%1'，请输入:").arg(input));
        emit SendMessage(QString("  - 'continue' 或 'c' 继续"));
        emit SendMessage(QString("  - 'quit' 或 'q' 退出"));
    }
}


// ******************************************************************************
