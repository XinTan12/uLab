#include "uLab.h"

ULab::ULab(QObject *parent)
    : QObject(parent)
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
    if (pPort->open(QIODevice::ReadWrite)) {
        pPortTimer->start(CMD_INTERVAL);
        pParseTimer->start(PARSE_INTERVAL);
        pReadTimer->start(READ_INTERVAL);
        pGetFlowTimer->start(FLOW_INTERVAL);
        emit SendMessage("Succeed in connecting " + portName);
        return true;
    } else {
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
    emit SendMessage("Peristaltic pump "
                     + (start ? (QString("start to rotate in ") + (direction ? "normal" : "reverse")
                                 + " direction")
                              : " stop rotating"));
}

void ULab::SetSpeed(uint16_t speed, uint8_t id)
{
    wrtCmdList.append(GenCMD(0x09, id, speed >> 8, speed & 0xff));
    emit SendMessage("Set peristaltic pump speed to " + QString::number(speed));
}

void ULab::GotoHole(uint8_t addr, uint8_t hole, uint8_t id)
{
    wrtCmdList.append(GenCMD(0x08, id, hole, addr));
    emit SendMessage("Valve (Address:" + QString::number(addr) + " go to hole No."
                     + QString::number(hole));
}

void ULab::Home(AXIS axis, DEVICE_CODE id)
{
    wrtCmdList.append(GenCMD(axis, id, 0x00, 0x00));
    emit SendMessage(GetAxisName(axis) + " of low-precision table go back to home");
}

void ULab::Goto(AXIS axis, uint16_t pos, DEVICE_CODE id)
{
    //    if (pos >= (1<<17)-1)
    //    {
    //        emit SendMessage(GetAxisName(axis) + " of low-precision table's destination is out of range!");
    //        return;
    //    }
    //    uint16_t pos_ = pos / 2;
    wrtCmdList.append(GenCMD(1 + axis, id, pos >> 8, pos & 0xff));
    emit SendMessage(GetAxisName(axis) + " of "
                     + (id == LOW_STAGE_CODE ? "low-precision" : "high-precision") + "table go to "
                     + QString::number(pos));
}

void ULab::SetSpeedStage(AXIS axis, uint16_t speed, DEVICE_CODE id)
{
    wrtCmdList.append(GenCMD(2 + axis, id, speed >> 8, speed & 0xff));
}

void ULab::SetTime(AXIS axis, uint16_t time, DEVICE_CODE id)
{
    wrtCmdList.append(GenCMD(3 + axis, id, time >> 8, time & 0xff));
}

void ULab::Go(AXIS axis, bool direction, DEVICE_CODE id)
{
    wrtCmdList.append(GenCMD(4 + axis, id, direction ? 0x01 : 0x00, 0x00));
}

void ULab::Enable(AXIS axis, bool enable, DEVICE_CODE id)
{
    wrtCmdList.append(GenCMD(5 + axis, id, enable ? 0x00 : 0x01, 0x00));
}

void ULab::GetPos(AXIS axis, DEVICE_CODE id)
{
    wrtCmdList.append(GenCMD(7 + axis, id, 1 + axis, 0x00));
}

void ULab::SetAxisEnable(AXIS axis, bool enable, DEVICE_CODE code)
{
    Enable(axis, enable, code);
    if (code == LOW_STAGE_CODE) {
        if (enable) {
            switch (axis) {
            case AXIS_X:
                connect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosLowX);
                break;
            case AXIS_Y:
                connect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosLowY);
                break;
            case AXIS_Z:
                connect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosLowZ);
                break;
            default:;
            }
        } else {
            switch (axis) {
            case AXIS_X:
                disconnect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosLowX);
                break;
            case AXIS_Y:
                disconnect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosLowY);
                break;
            case AXIS_Z:
                disconnect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosLowZ);
                break;
            default:;
            }
        }
    } else if (code == HIGH_STAGE_CODE) {
        if (enable) {
            switch (axis) {
            case AXIS_X:
                connect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosHighX);
                break;
            case AXIS_Y:
                connect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosHighY);
                break;
            case AXIS_Z:
                connect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosHighZ);
                break;
            default:;
            }
        } else {
            switch (axis) {
            case AXIS_X:
                disconnect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosHighX);
                break;
            case AXIS_Y:
                disconnect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosHighY);
                break;
            case AXIS_Z:
                disconnect(pReadTimer, &QTimer::timeout, this, &ULab::GetPosHighZ);
                break;
            default:;
            }
        }
    }
}

void ULab::SetFluigentEnable(bool enable)
{
    if (enable) {
        connect(pGetFlowTimer, &QTimer::timeout, this, &ULab::GetPresAndFlow);
    } else {
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
    wrtCmdList.append(GenCMD(0x51, PUMP_CODE, speed >> 8, speed & 0xff));
}

void ULab::SetSolenoidValve(uint8_t valves)
{
    wrtCmdList.append(GenCMD(0x41, PUMP_CODE, valves, 0x00));
}

void ULab::RefreshPort()
{
    if (!wrtCmdList.isEmpty()) {
        pPort->write(wrtCmdList.takeFirst());
    }
}

void ULab::ParsePort()
{
    readBuffer += pPort->readAll();

    for (int index = 0;; index++) {
        if (index + 7 >= readBuffer.length()) {
            break;
        }
        if (readBuffer.at(index) == (char) 0xFE && readBuffer.at(index + 7) == (char) 0xFF) {
            QByteArray cmd = readBuffer.mid(index, 8);
            readBuffer.remove(index--, 8);
            if (!CheckCMD(cmd)) {
                continue;
            }
            switch (cmd.at(2)) {
            case PIPET_CODE:
                break;
            case LOW_STAGE_CODE:  //低精度位移台回复指令
            case HIGH_STAGE_CODE: //高精度位移台回复指令
            {
                uint pos = ((uint) (uint8_t) cmd.at(3) << 8) + cmd.at(4);
                emit UpdatePos((DEVICE_CODE) cmd.at(2), (AXIS) (cmd.at(1) - 7), pos);
                break;
            }
            case PUMP_CODE: //气泵回复指令
            {
                if (cmd.at(1) == 0x24) {
                    uint pressure = ((uint) (uint8_t) cmd.at(3) << 8) + cmd.at(4);
                    emit UpdatePressure(pressure);
                } else if (cmd.at(1) == 0x23) {
                    uint flow = ((uint) (uint8_t) cmd.at(3) << 8) + cmd.at(4);
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
    QByteArray cmd = QByteArray::fromHex("FE").append(code).append(id).append(contentH).append(
        contentL);
    cmd.append(CRCMDBS_GetValue(cmd)).append(0xFF);
    return cmd;
}

bool ULab::CheckCMD(QByteArray cmd)
{
    if (cmd.length() != 8 || cmd.at(0) != (char) 0xFE || cmd.at(7) != (char) 0xFF) {
        return false;
    }
    if (CRCMDBS_GetValue(cmd.left(5)) != cmd.mid(5, 2)) {
        return false;
    }
    return true;
}

//CRC高位字节值表
const unsigned char CRC_High[]
    = {0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
       0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
       0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
       0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
       0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81,
       0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
       0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
       0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
       0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
       0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
       0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
       0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
       0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
       0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
       0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
       0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
       0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
       0x40};
//CRC 低位字节值表
const unsigned char CRC_Low[]
    = {0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0x05, 0xC5, 0xC4,
       0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
       0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD,
       0x1D, 0x1C, 0xDC, 0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
       0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36, 0xF6, 0xF7,
       0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
       0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE,
       0x2E, 0x2F, 0xEF, 0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
       0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1, 0x63, 0xA3, 0xA2,
       0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
       0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB,
       0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
       0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0, 0x50, 0x90, 0x91,
       0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
       0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88,
       0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
       0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41, 0x81, 0x80,
       0x40};

QByteArray CRCMDBS_GetValue(QByteArray msg)
{
    unsigned char m_CRC_High = 0xFF; //高CRC 字节初始化
    unsigned char m_CRC_Low = 0xFF;  //低CRC 字节初始化
    unsigned char uIndex;            //CRC 循环中的索引
    for (int index = 0; index < msg.size(); ++index) {
        uIndex = m_CRC_Low ^ msg.at(index);
        m_CRC_Low = m_CRC_High ^ CRC_High[uIndex];
        m_CRC_High = CRC_Low[uIndex];
    }
    return QByteArray().append(m_CRC_High).append(m_CRC_Low);
    //    return (m_CRC_High<<8|m_CRC_Low);
}

QString GetAxisName(AXIS axis)
{
    switch (axis) {
    case AXIS_X:
        return "AXIS X";
    case AXIS_Y:
        return "AXIS Y";
    case AXIS_Z:
        return "AXIS Z";
    default:
        return "";
    }
}

void MSleep(uint msec)
{
    QEventLoop loop;
    QTimer::singleShot(msec, &loop, SLOT(quit()));
    loop.exec();
}
