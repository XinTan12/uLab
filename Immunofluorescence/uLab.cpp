#include "uLab.h"
#include <QCoreApplication>
#include <QtMath>

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
    connect(pPortTimer, &QTimer::timeout, this, &ULab::RefreshPort);
    connect(pParseTimer, &QTimer::timeout, this, &ULab::ParsePort);
    //    pCRC = new CRC();
}

ULab::~ULab()
{
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
    emit SendMessage("Peristaltic pump " + (start ? (QString("start to rotate in ") +
                                                     (direction ? "normal" : "reverse") + " direction") : " stop rotating"));
}

void ULab::SetSpeed(uint16_t speed, uint8_t id)
{
    wrtCmdList.append(GenCMD(0x09, id, speed >> 8, speed & 0xff));
    emit SendMessage("Set peristaltic pump speed to " + QString::number(speed));
}

void ULab::GotoHole(uint8_t addr, uint8_t hole, uint8_t id)
{
    wrtCmdList.append(GenCMD(0x08, id, hole, addr));
    emit SendMessage("Valve (Address:" + QString::number(addr) + " go to hole No." + QString::number(hole));
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

void MSleep(uint msec)
{
    QEventLoop loop;
    QTimer::singleShot(msec, &loop, SLOT(quit()));
    loop.exec();
}


// ******************************* 低精度位移台运动控制 *********************************

// ********************************* 定向移动运动控制 **********************************


void ULab::MoveStage(DEVICE_CODE stage_type,
                     QPoint target_start_pos,
                     AXIS direction,
                     bool positive,
                     int total_steps,
                     int speed_x,
                     int speed_y,
                     int dwell_ms) // dwell_ms 是X/Y轴的停留时间
{
    m_emergencyFlag.storeRelaxed(0);

    if(!STAGE_CONFIG.contains(stage_type))
    {
        emit SendMessage("错误：未知设备类型!");
        return;
    }
    auto params = STAGE_CONFIG[stage_type];

    emit SendMessage("开始Z轴归位/回到初始安全位置...");
    Home(AXIS_Z, params.code);

    // 检查是否需要移动到起始位置
    QPoint current_pos_xy = m_currentPos.value(stage_type, QPoint(-1,-1)); // 仅用于X, Y
    if(current_pos_xy != target_start_pos)
    {
        if(m_emergencyFlag.loadRelaxed())
        {
            emit SendMessage("急停激活，校准取消");
            return;
        }

        emit SendMessage(QString("当前位置(%1,%2)与目标起始位置(%3,%4)不一致，开始校准...")
                             .arg(current_pos_xy.x()).arg(current_pos_xy.y())
                             .arg(target_start_pos.x()).arg(target_start_pos.y()));

        // X轴校准
        if(target_start_pos.x() != current_pos_xy.x())
        {
            Goto(AXIS_X, static_cast<uint16_t>(target_start_pos.x() * params.x_step_um), params.code);
            MSleep(1500); // 等待X轴移动
        }

        // Y轴校准
        if(target_start_pos.y() != current_pos_xy.y())
        {
            Goto(AXIS_Y, static_cast<uint16_t>(target_start_pos.y() * params.y_step_um), params.code);
            MSleep(1500); // 等待Y轴移动
        }
        m_currentPos[stage_type] = target_start_pos; // 更新X,Y当前位置
    }

    // uint16_t actual_x_speed = (speed_x > 0) ? static_cast<uint16_t>(speed_x) : params.x_speed;
    // uint16_t actual_y_speed = (speed_y > 0) ? static_cast<uint16_t>(speed_y) : params.y_speed;
    uint16_t actual_z_speed = params.z_speed;

    // SetSpeedStage(AXIS_X, actual_x_speed, params.code);
    // SetSpeedStage(AXIS_Y, actual_y_speed, params.code);
    SetSpeedStage(AXIS_Z, actual_z_speed, params.code);
    MSleep(100); // 等待速度设置指令发送

    // 假设Z轴的初始/原点位置为0um。如果不是，需要调整。
    const uint16_t z_original_pos_um = 0;
    const uint16_t z_travel_um = Z_AXIS_TRAVEL_MM * 1000;
    const uint16_t z_down_pos_um = z_original_pos_um + z_travel_um;


    // 计算Z轴移动所需时间 (ms)
    // Z轴速度单位是 0.12 mm/s. s = actual_z_speed.
    // time_seconds = Z_AXIS_TRAVEL_MM / (actual_z_speed * 0.12)
    // time_ms = time_seconds * 1000

    int z_move_duration_ms = 0;
    if (actual_z_speed > 0 && params.code != PUMP_CODE)  // PUMP_CODE不适用位移台速度计算
    {
        double speed_mm_per_s = actual_z_speed * 0.12;
        if (speed_mm_per_s > 0) {
            z_move_duration_ms = static_cast<int>(qCeil((static_cast<double>(Z_AXIS_TRAVEL_MM) / speed_mm_per_s) * 1000.0));
        } else {
            z_move_duration_ms = 3000; // 默认延时
        }
    }
    else
    {
        z_move_duration_ms = 3000; // 默认延时
    }
    if (z_move_duration_ms < 500) z_move_duration_ms = 500; // 最小延时

    // 分步移动
    for(int step = 0; step < total_steps; ++step)
    {
        if(m_emergencyFlag.loadAcquire())
        {
            emit SendMessage("移动被急停中断");
            return;
        }
        QCoreApplication::processEvents();

        QPoint new_xy_pos = m_currentPos[stage_type];
        switch(direction)
        {
        case AXIS_X:
            new_xy_pos.setX(positive ? new_xy_pos.x() + 1 : new_xy_pos.x() - 1);
            break;
        case AXIS_Y:
            new_xy_pos.setY(positive ? new_xy_pos.y() + 1 : new_xy_pos.y() - 1);
            break;
        default:
            emit SendMessage("无效运动轴向!");
            return;
        }

        if(new_xy_pos.x() < 0 || new_xy_pos.x() >= params.cols ||
            new_xy_pos.y() < 0 || new_xy_pos.y() >= params.rows)
        {
            emit SendMessage("移动超出孔板范围!");
            return;
        }

        // 执行X或Y轴单步移动
        uint16_t target_xy_pos_um = (direction == AXIS_X ? static_cast<uint16_t>(new_xy_pos.x() * params.x_step_um)
                                                         : static_cast<uint16_t>(new_xy_pos.y() * params.y_step_um));
        Goto(direction, target_xy_pos_um, params.code);
        MSleep(800); // 等待X或Y轴移动完成 (这个延时需要根据实际情况调整)

        m_currentPos[stage_type] = new_xy_pos; // 更新X,Y当前位置
        emit SendMessage(QString("[%1] X/Y轴已到达步骤%2/%3 - 位置(%4,%5)")
                             .arg(stage_type == LOW_STAGE_CODE ? "低精度" : "高精度")
                             .arg(step+1).arg(total_steps)
                             .arg(new_xy_pos.x()).arg(new_xy_pos.y()));

        // --- Z轴操作开始 ---
        if(m_emergencyFlag.loadAcquire())
        { emit SendMessage("Z轴操作前急停"); return; }
        emit SendMessage(QString("Z轴开始向下移动 %1 mm").arg(Z_AXIS_TRAVEL_MM));
        Goto(AXIS_Z, z_down_pos_um, params.code);
        MSleep(z_move_duration_ms + 200); // 等待Z轴向下移动完成，额外200ms缓冲

        if(m_emergencyFlag.loadAcquire())
        { emit SendMessage("Z轴向下移动后急停"); return; }
        emit SendMessage(QString("Z轴在底部停留 %1 ms").arg(Z_AXIS_DWELL_MS));
        MSleep(Z_AXIS_DWELL_MS);

        if(m_emergencyFlag.loadAcquire())
        { emit SendMessage("Z轴停留后急停"); return; }
        emit SendMessage("Z轴开始向上移动到原位");
        Goto(AXIS_Z, z_original_pos_um, params.code);
        MSleep(z_move_duration_ms + 200); // 等待Z轴向上移动完成，额外200ms缓冲
        // --- Z轴操作结束 ---

        // X/Y轴的停留时间
        if (dwell_ms > 0) {
            if(m_emergencyFlag.loadAcquire()) { emit SendMessage("X/Y停留前急停"); return; }
            emit SendMessage(QString("X/Y轴在位置(%1,%2)停留 %3 ms").arg(new_xy_pos.x()).arg(new_xy_pos.y()).arg(dwell_ms));
                // 使用循环和processEvents允许在长延时期间响应急停
            for(int i = 0; i < dwell_ms / 100; ++i) {
                if(m_emergencyFlag.loadAcquire()) {
                    emit SendMessage("X/Y停留期间触发急停");
                    return;
                }
                MSleep(100);
                QCoreApplication::processEvents();
            }
            if (dwell_ms % 100 > 0) { // 处理余下的延时
                MSleep(dwell_ms % 100);
            }
        }
    }
    emit SendMessage(QString("[%1] 定向移动完成").arg(stage_type == LOW_STAGE_CODE ? "低精度" : "高精度"));
}

// **********************************************************************************

// ********************************* 全板遍历运动控制 **********************************


void ULab::MoveStage(DEVICE_CODE stage_type, int speed_x, int speed_y, int dwell_ms) // dwell_ms,停留时间，即加液时间
{
    m_emergencyFlag.storeRelaxed(0); // 重置急停标志

    if(!STAGE_CONFIG.contains(stage_type)) {
        emit SendMessage("错误：未知设备类型!");
        return;
    }
    auto params = STAGE_CONFIG[stage_type];

    // uint16_t actual_x_speed = (speed_x > 0) ? static_cast<uint16_t>(speed_x) : params.x_speed;
    // uint16_t actual_y_speed = (speed_y > 0) ? static_cast<uint16_t>(speed_y) : params.y_speed;
    // uint16_t actual_z_speed = params.z_speed;

    // SetSpeedStage(AXIS_X, actual_x_speed, params.code);
    // SetSpeedStage(AXIS_Y, actual_y_speed, params.code);
    // SetSpeedStage(AXIS_Z, actual_z_speed, params.code);
    // MSleep(100); // 等待速度设置指令发送

    // 归位X和Y轴到物理原点 (0,0)
    // emit SendMessage("开始X轴归位...");
    // Home(AXIS_X, params.code);
    // MSleep(5000);
    // emit SendMessage("开始Y轴归位...");
    // Home(AXIS_Y, params.code);
    // MSleep(5000);
    // emit SendMessage("开始Z轴归位...");
    // Home(AXIS_Z, params.code);
    // MSleep(5000);

    m_currentPos[stage_type] = QPoint(-1,-1); // 归位后，逻辑孔位为-1,-1 (在A1之前)

    // const uint16_t z_original_pos_um = 0; // 假设Z轴归位后为0um
    // const uint16_t z_travel_um = Z_AXIS_TRAVEL_MM * 1000;
    // const uint16_t z_down_pos_um = z_original_pos_um + z_travel_um;

    // int z_move_duration_ms = 0;
    // if (actual_z_speed > 0 && params.code != PUMP_CODE) {
    //     double speed_mm_per_s = actual_z_speed * 0.12;
    //     if (speed_mm_per_s > 0) {
    //         z_move_duration_ms = static_cast<int>(qCeil((static_cast<double>(Z_AXIS_TRAVEL_MM) / speed_mm_per_s) * 1000.0));
    //     } else {
    //         z_move_duration_ms = 3000;
    //     }
    // } else {
    //     z_move_duration_ms = 3000;
    // }
    // if (z_move_duration_ms < 500) z_move_duration_ms = 500;

    emit SendMessage("--- 开始初始定位：移动到A1点 ---");

    const int max_retries = 3; // 定义最大重试次数
    int retry_count = 0;       // 初始化重试计数器
    bool x_ok = false;         // 初始化X轴成功标志
    bool y_ok = false;         // 初始化Y轴成功标志
    int last_x = -1, last_y = -1; // 定义变量以接收当前坐标

    uint16_t a1_target_x = params.initial_offset_x_um;
    uint16_t a1_target_y = params.initial_offset_y_um;

    // 进入重试循环，直到成功或达到最大次数
    while (retry_count < max_retries) {
        if (retry_count > 0) {
            emit SendMessage(QString("!!! 定位失败，正在进行第 %1/%2 次重试...").arg(retry_count).arg(max_retries -1));
            emit SendMessage(QString("    目标 -> X: %1, Y: %2").arg(a1_target_x).arg(a1_target_y));
            emit SendMessage(QString("    当前 -> X: %1, Y: %2").arg(last_x).arg(last_y));
        }

        // 每次重试都重新发送移动指令
        Goto(AXIS_X, a1_target_x, params.code);
        MSleep(5000);
        Goto(AXIS_Y, a1_target_y, params.code);
        MSleep(5000);

        // 调用函数进行位置确认
        x_ok = waitForPosition(stage_type, AXIS_X, a1_target_x, last_x);
        y_ok = waitForPosition(stage_type, AXIS_Y, a1_target_y, last_y);

        // 如果两轴都已到位，则成功，跳出重试循环
        if (x_ok && y_ok) {
            break;
        }

        // 如果失败，增加计数器，准备下一次重试
        retry_count++;
        MSleep(500); // 重试前短暂延时
    }

    // 在循环结束后，最终检查是否成功。如果不成功，则说明所有重试都已失败。
    if (!x_ok || !y_ok) {
        emit SendMessage("!!! 达到最大重试次数，定位A1彻底失败，流程中止 !!!");
        emit SendMessage(QString("    最后状态 -> 目标X: %1, 当前X: %2 | 目标Y: %3, 当前Y: %4")
                             .arg(a1_target_x).arg(last_x).arg(a1_target_y).arg(last_y));
        return;
    }

    // --- A1点的特殊处理逻辑 ---
    emit SendMessage("已成功到达A1点，1秒后开始加液遍历运动");
    MSleep(1000);

    // A1点的加液操作 (Z轴)
    // emit SendMessage(QString("Z轴下降加液..."));
    // Goto(AXIS_Z, z_down_pos_um, params.code);
    // MSleep(z_move_duration_ms + 200);
    // emit SendMessage(QString("Z轴在底部停留 %1 ms").arg(Z_AXIS_DWELL_MS));
    // MSleep(Z_AXIS_DWELL_MS);
    // emit SendMessage(QString("Z轴上升..."));
    // Goto(AXIS_Z, z_original_pos_um, params.code);
    // MSleep(z_move_duration_ms + 200);

    // A1点的停留
    if (dwell_ms > 0) {
        emit SendMessage(QString("在孔位 A1 等待 %2 ms").arg(dwell_ms));
        MSleep(dwell_ms);
    }


    // 蛇形遍历算法
    // 外层循环控制X轴，对应孔板的“行” (A, B, C...)
    for(int row = 0; row < params.rows; ++row) {
        if(m_emergencyFlag.loadAcquire()) { emit SendMessage("遍历被急停中断"); return; }
        QCoreApplication::processEvents();

        // 1. 每换一行，先移动X轴到目标行的位置
        uint16_t target_x_um = params.initial_offset_x_um + (row * params.x_step_um);
        emit SendMessage(QString("移动到第 %1 行 (X坐标: %2 um)").arg(QChar('A' + row)).arg(target_x_um));
        Goto(AXIS_X, target_x_um, params.code);
        if (!waitForPosition(stage_type, AXIS_X, target_x_um, last_x)) return; // 每次换行都确认X轴位置

        // 2. 根据行的奇偶，决定内层Y轴循环的方向
        bool left_to_right = (row % 2 == 0); // 偶数行 (A, C, E...) 的列号从小到大

        // 内层循环控制Y轴，对应孔板的“列” (1, 2, 3...)
        int col_start = left_to_right ? 0 : params.cols - 1;
        int col_end = left_to_right ? params.cols : -1;
        int col_step = left_to_right ? 1 : -1;

        // 对第一行 (A行) 的特殊处理，使其从A2 (col=1) 开始
        if (row == 0) {
            col_start = 1;
        }

        for(int col = col_start; col != col_end; col += col_step) {
            if(m_emergencyFlag.loadAcquire()) { emit SendMessage("遍历被急停中断"); return; }
            // QCoreApplication::processEvents();

            // 2.1. Y轴定位到当前列
            uint16_t target_y_um = params.initial_offset_y_um - (col * params.y_step_um);
            Goto(AXIS_Y, target_y_um, params.code);
            if (!waitForPosition(stage_type, AXIS_Y, target_y_um, last_y)) return; // 每次换列都确认Y轴位置

            // 2.2. 发送消息和处理特殊延时
            QString wellName = getWellName(row, col);
            emit SendMessage(QString("已运动到%1点，开始加液").arg(wellName));

            // 2.3. Z轴操作 (加液)
            // emit SendMessage(QString("Z轴下降加液..."));
            // Goto(AXIS_Z, z_down_pos_um, params.code);
            // MSleep(z_move_duration_ms + 200);

            // emit SendMessage(QString("Z轴在底部停留 %1 ms").arg(Z_AXIS_DWELL_MS));
            // MSleep(Z_AXIS_DWELL_MS);

            // emit SendMessage(QString("Z轴上升..."));
            // Goto(AXIS_Z, z_original_pos_um, params.code);
            // MSleep(z_move_duration_ms + 200);

            // 2.4. 停留
            if (dwell_ms > 0) {
                emit SendMessage(QString("在孔位 %1 等待 %2 ms").arg(wellName).arg(dwell_ms));
                MSleep(dwell_ms);
            }
        }
    }
    emit SendMessage(QString("[%1] 全板遍历完成").arg(stage_type == LOW_STAGE_CODE ? "低精度" : "高精度"));
}



// **************************************************************************


void ULab::SendData(const QByteArray &data)
{
    if (pPort && pPort->isOpen()) {
        pPort->write(data);
        pPort->flush();
    }
}

void ULab::EmergencyStop() {
    m_emergencyFlag.storeRelaxed(1); // 设置急停标志

    // 发送急停指令到所有设备
    QStringList stopCommands;
    stopCommands << "AXIS_X STOP_EMERGENCY"
                 << "AXIS_Y STOP_EMERGENCY"
                 << "SAVE_EMERGENCY_STATE";

    // 通过串口发送急停指令
    foreach(const auto& cmd, stopCommands) {
        SendData(cmd.toLatin1());
        MSleep(50);
    }

    // 禁用所有轴使能
    SetAxisEnable(AXIS_X, false, LOW_STAGE_CODE);
    SetAxisEnable(AXIS_Y, false, LOW_STAGE_CODE);
    SetAxisEnable(AXIS_X, false, HIGH_STAGE_CODE);
    SetAxisEnable(AXIS_Y, false, HIGH_STAGE_CODE);

    wrtCmdList.clear();

    // 立即刷新串口
    if(pPort->isOpen())
    {
        pPort->flush();
    }

    emit EmergencyStopTriggered();
    emit SendMessage("! 紧急停止已触发 !");
}


bool ULab::waitForPosition(DEVICE_CODE stage_type, AXIS axis, uint16_t target_pos, int& last_pos, int timeout_ms)
{
    emit SendMessage(QString("正在确认 %1... 目标: %2").arg(GetAxisName(axis)).arg(target_pos));

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    // 连接超时信号，如果超时，循环将以状态码1退出
    connect(&timer, &QTimer::timeout, &loop, [&](){ loop.exit(1); });

    bool position_reached = false;
    last_pos = -1; // 初始化为-1，表示尚未收到任何位置信息
    // 连接位置更新信号
    auto conn = connect(this, &ULab::UpdatePos, this,
                        [&](DEVICE_CODE id, AXIS a, int pos) {
                            // 检查是否是我们关心的设备和轴
                            if (id == stage_type && a == axis) {
                                last_pos = pos;    // 无论是否到达，都更新最后的位置
                                // 检查坐标是否在目标1000微米的容差范围内
                                if (qAbs(pos - target_pos) < 1000) {
                                    position_reached = true;
                                    loop.exit(0); // 成功，以返回码 0 退出
                                }
                            }
                        });

    // 创建一个定时器，周期性地发送查询位置的指令
    QTimer pollTimer;
    connect(&pollTimer, &QTimer::timeout, this, [this, axis, stage_type](){
        GetPos(axis, stage_type);
    });

    pollTimer.start(250); // 每250毫秒查询一次位置
    timer.start(timeout_ms); // 启动总超时定时器

    int exit_code = loop.exec(); // 开始事件循环，程序会在这里“等待”

    pollTimer.stop();
    disconnect(conn); // 清理信号连接

    if (exit_code != 0) { // exit_code不为0，意味着超时失败
        // 失败时，last_pos中保存的是超时前最后一次收到的坐标
        emit SendMessage(QString("错误: %1 未能在 %2ms 内到达目标! (最后位置: %3)")
                             .arg(GetAxisName(axis)).arg(timeout_ms).arg(last_pos));
    }

    return position_reached; // position_reached只在成功时为true
}


// ******************************* 多通道换液流程 *********************************


void ULab::Pump_Peristaltic(uint8_t id, bool direction, double flow_speed, double volume_ul)
{
    // 速度单位转换因子：每 1 µL/s 的流速对应多少设备速度单位 (例如 2.0 RPMs per µL/s)
    const double SPEED_UNIT_CONVERSION_FACTOR = 2.0;


    if (flow_speed <= 0) {
        emit SendMessage("  > 错误：流速为0，无法计算时长。");
        return;
    }
    uint16_t hardware_speed = static_cast<uint16_t>(flow_speed * SPEED_UNIT_CONVERSION_FACTOR);
    uint duration_ms = static_cast<uint>((volume_ul / flow_speed) * 1000.0);

    emit SendMessage(QString("  > 参数: 流速=%1uL/s, 体积=%2uL, 计算时长=%3ms")
                         .arg(flow_speed).arg(volume_ul).arg(duration_ms));

    SetSpeed(hardware_speed, id);
    Rotate(true, direction, id);
    MSleep(duration_ms);
    Rotate(false, direction, id);
}

void ULab::Pump_in(const Setconfig_Pump_in& config)
{
    // 阀门切换延时
    const int VALVE_SWITCH_DELAY_MS = 1000;

    // 从开始到最终加液端口的液体“死体积”大小
    const int DEAD_VOLUME = 500;

    emit SendMessage(QString("\n[执行动作]: %1").arg(config.action_name));

    emit SendMessage(QString("  > 配置线路: [源] %1号切换阀-%2通道 --> [%3号泵] --> [目标] %4号切换阀-%5通道")
                         .arg(config.valve_id_in).arg(config.channel_in)
                         .arg(config.pump_id)
                         .arg(config.valve_id_out).arg(config.channel_out));

    GotoHole(0x00, config.channel_in, config.valve_id_in);
    MSleep(VALVE_SWITCH_DELAY_MS);
    GotoHole(0x00, config.channel_out, config.valve_id_out);
    MSleep(VALVE_SWITCH_DELAY_MS);

    Pump_Peristaltic(config.pump_id, config.isForward, config.speed, config.volume_ul+DEAD_VOLUME);

    emit SendMessage(QString("  > 动作 '%1' 完成.").arg(config.action_name));
}


// ******************************************************************************
