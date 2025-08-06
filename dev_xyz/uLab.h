#ifndef ULAB_H
#define ULAB_H

#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QTime>
#include <QEventLoop>
#include <QMap>
#include <QPoint>
#include <QAtomicInteger>
//#include "CRC.h"

#define CMD_INTERVAL    100             //发送串口指令间隔，单位：ms
#define PARSE_INTERVAL  30             //解析收到指令间隔，单位：ms
#define READ_INTERVAL   1000           //发送查询指令间隔，单位：ms
#define FLOW_INTERVAL   6000           //发送查询气压和流量指令间隔，单位：ms
#define Z_AXIS_TRAVEL_MM 30            // Z轴移动距离 (mm)
#define Z_AXIS_DWELL_MS  1000          // Z轴在底部停留时间 (ms)

enum DEVICE_CODE
{
    PIPET_CODE = 0x01,
    LOW_STAGE_CODE = 0x02,
    HIGH_STAGE_CODE = 0x03,
    PUMP_CODE = 0x04,
};

enum AXIS
{
    AXIS_X = 0x01,
    AXIS_Y = 0x09,
    AXIS_Z = 0x11,
};

struct StageParams {
    int rows;
    int cols;
    int x_step_um;      // X轴孔间距 (µm)
    int y_step_um;      // Y轴孔间距 (µm)
    uint16_t x_speed;   // X轴速度（单位：0.12mm/s）
    uint16_t y_speed;   // Y轴速度
    uint16_t z_speed;   // Z轴速度
    int initial_offset_x_um; // 从位移台原点到A1孔的X方向距离(µm)
    int initial_offset_y_um; // 从位移台原点到A1孔的Y方向距离
    DEVICE_CODE code;
};

QByteArray CRCMDBS_GetValue(QByteArray msg);
QString GetAxisName(AXIS axis);

void MSleep(uint msec);             //非阻塞延时

class ULab : public QObject
{
    Q_OBJECT
public:
    explicit ULab(QObject *parent = nullptr);
    ~ULab();

    bool InitPort(QString portName);
    void ClosePort();

    // Pipet
    void Rotate(bool start = true, bool direction = true, uint8_t id = 1);
    void SetSpeed(uint16_t speed, uint8_t id = 1);
    void GotoHole(uint8_t addr, uint8_t hole, uint8_t id = 1);

    // xyz stages
    void Home(AXIS axis, DEVICE_CODE code = LOW_STAGE_CODE);
    void Goto(AXIS axis, uint16_t pos, DEVICE_CODE code = LOW_STAGE_CODE);                   //unit of pos: um
    void SetSpeedStage(AXIS axis, uint16_t speed, DEVICE_CODE code = LOW_STAGE_CODE);        //unit of speed: 0.12 mm/s
    void SetTime(AXIS axis, uint16_t time, DEVICE_CODE code = LOW_STAGE_CODE);               //unit of time: ms
    void Go(AXIS axis, bool direction, DEVICE_CODE code = LOW_STAGE_CODE);
    void Enable(AXIS axis, bool enable, DEVICE_CODE code = LOW_STAGE_CODE);
    void GetPos(AXIS axis, DEVICE_CODE code = LOW_STAGE_CODE);
    void SetAxisEnable(AXIS axis, bool enable = false, DEVICE_CODE code = LOW_STAGE_CODE);
    void SetFluigentEnable(bool enable = false);
    void GetPressure();
    void GetFlow();

    // Pump
    void StartPump(uint16_t speed);
    void StopPump();
    void SetPressure(uint16_t pressure);
    void SetFlow(uint16_t flow);
    void PeristalticPumpRotate(bool start = true);                                          //气泵控制板连接的蠕动泵启动/停止
    void PeristalticPumpSetSpeed(uint16_t speed);                                           //气泵控制板连接的蠕动泵设置转速
    void SetSolenoidValve(uint8_t valves);
    void PerformLiquidExchange(uint8_t id,
                               const QList<uint8_t>& channels,
                               uint16_t pump_speed,
                               uint pumping_duration_ms,
                               uint8_t valve_addr,
                               bool direction);

    //低精度位移台运动控制
    void MoveStage(DEVICE_CODE stage_type,                                                  // 根据输入参数定向移动
                   QPoint start_pos,
                   AXIS direction,
                   bool positive,
                   int steps,
                   int speed_x = -1,
                   int speed_y = -1,
                   int dwell_ms = 1000);

    void MoveStage(DEVICE_CODE stage_type,
                   int speed_x = -1,
                   int speed_y = -1,
                   int dwell_ms = 1000);                                                       // 全板遍历

    void EmergencyStop();
    void SendData(const QByteArray &data);


signals:
    void SendMessage(QString msg);
    void UpdatePos(DEVICE_CODE id, AXIS axis, int pos);                                     //通知主界面更新位置信息
    void UpdatePressure(uint pressure);                                                     //通知主界面更新气压信息
    void UpdateFlow(uint flow);                                                             //通知主界面更新流量信息

    void EmergencyStopTriggered();

private slots:
    void RefreshPort();
    void ParsePort();
    void GetPosLowX()
    {
        GetPos(AXIS_X);
    }
    void GetPosLowY()
    {
        GetPos(AXIS_Y);
    }
    void GetPosLowZ()
    {
        GetPos(AXIS_Z);
    }
    void GetPosHighX()
    {
        GetPos(AXIS_X, HIGH_STAGE_CODE);
    }
    void GetPosHighY()
    {
        GetPos(AXIS_Y, HIGH_STAGE_CODE);
    }
    void GetPosHighZ()
    {
        GetPos(AXIS_Z, HIGH_STAGE_CODE);
    }
    void GetPresAndFlow();

private:
    QSerialPort* pPort;
    QByteArray GenCMD(uint8_t code, uint8_t id, uint8_t contentH, uint8_t contentL);
    bool CheckCMD(QByteArray cmd);
    QString portName;
    QTimer *pPortTimer;
    QTimer *pParseTimer;
    QTimer *pReadTimer;
    QTimer *pGetFlowTimer;
    QList<QByteArray> wrtCmdList;
    QByteArray readBuffer;
    QMap<DEVICE_CODE, QPoint> m_currentPos;
    QAtomicInt m_emergencyFlag{0}; // 原子操作的急停标志


    bool waitForPosition(DEVICE_CODE stage_type, AXIS axis, uint16_t target_pos, int& last_pos, int timeout_ms = 10000);

    // 设备参数配置
    const QMap<DEVICE_CODE, StageParams> STAGE_CONFIG =
    {
        //                rows,cols, x_step, y_step, x_spd,y_spd,z_spd, offset_x, offset_y, code
        {LOW_STAGE_CODE,  {8,   12,   4500,   4500,  20,   20,   100,   14000,    50000,    LOW_STAGE_CODE}},
        {HIGH_STAGE_CODE, {8,   12,   450,    450,   20,   20,   20,    10000,    10000,    HIGH_STAGE_CODE}}
    };


};

#endif // ULAB_H
