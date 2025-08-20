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

#define CMD_INTERVAL            100             //发送串口指令间隔，单位：ms
#define PARSE_INTERVAL          30             //解析收到指令间隔，单位：ms
#define READ_INTERVAL           1000           //发送查询指令间隔，单位：ms
#define FLOW_INTERVAL           6000           //发送查询气压和流量指令间隔，单位：ms

#define REAGENT_VALVE_ADDR      0      // 第一个切换阀地址 (连接试剂)
#define SAMPLE_VALVE_ADDR       1      // 第二个切换阀地址 (连接样品)
#define PUMP_IN_ID              1      // 第一个蠕动泵ID (加液)
#define PUMP_OUT_ID             8      // 第一个蠕动泵ID (抽液)
#define VALVE_SWITCH_DELAY_MS   500   // 切换阀通道切换延时
#define DEAD_VOLUME             500    // 管路死体积

// 冲洗管路
#define WASH_SPEED                150.0    // 冲洗速度 (uL/s)
#define WASH_DURATION_SEC         15      // 冲洗持续时间 (秒)

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

enum FluidSpeed
{
    SLOW = 0,
    MEDIUM = 1,
    FAST = 2
};


QByteArray CRCMDBS_GetValue(QByteArray msg);
QString GetAxisName(AXIS axis);

struct Setconfig_Pump_in
{
    QString action_name;
    uint8_t valve_id_in;
    uint8_t channel_in;
    uint8_t pump_id;
    double speed;
    bool isForward;
    uint8_t valve_id_out;
    uint8_t channel_out;
    double volume_ul;
};

struct ReagentConfig
{
    QString reagent_name;
    uint8_t valve_channel;
};

struct SampleConfig
{
    QString sample_name;
    uint8_t valve_channel;
};

void MSleep(uint msec);                                      //阻塞延时
void MSleepInterruptible(uint msec);                         //可中断的延时

class ULab : public QObject
{
    Q_OBJECT
public:
    explicit ULab(QObject *parent = nullptr);
    ~ULab();

    bool InitPort(QString portName);
    void ClosePort();

    void EmergencyStop();
    void SendData(const QByteArray &data);

    // Pipet
    void Rotate(bool start = true, bool direction = true, uint8_t id = 1);
    void SetSpeed(uint16_t speed, uint8_t id = 1);
    void GotoChannel(uint8_t addr, uint8_t hole, uint8_t id = 1);

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

    void Pump_in(const Setconfig_Pump_in& config);
    void Pump_Peristaltic(uint8_t id, bool direction, double flow_speed, double volume_ul);
    
    void AddLiquid(const QString& reagent_name, double volume_ul, FluidSpeed speed, const QString& sample_name, uint delay_sec = 1);
    
    void WashPipeline(const QString& reagent_name, const QString& sample_name);
    
    void InitialWashPipelines();
    
    void performWash(uint8_t reagentChannel, uint8_t sampleChannel, uint8_t wasteChannel);
    
    void StopAllDevices();
    
    void WaitForUserInput(const QString& message);
    
    void SetReagentConfig(const QMap<QString, ReagentConfig>& config);
    void SetSampleConfig(const QMap<QString, SampleConfig>& config);
    void SetPumpInterval(uint interval_ms);

public slots:
    void onUserInputReceived(QString input);                                                //处理用户输入

signals:
    void SendMessage(QString msg);
    void UpdatePos(DEVICE_CODE id, AXIS axis, int pos);                                     //通知主界面更新位置信息
    void UpdatePressure(uint pressure);                                                     //通知主界面更新气压信息
    void UpdateFlow(uint flow);                                                             //通知主界面更新流量信息

    void EmergencyStopTriggered();
    void UserInputReceived(QString input);                                                  //用户输入

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
    QAtomicInt m_emergencyFlag{0};                                                          // 原子操作的急停标志
    QMap<QString, ReagentConfig> m_reagentConfigs;                                          // 试剂配置映射
    QMap<QString, SampleConfig> m_sampleConfigs;                                            // 样品配置映射
    uint m_pumpInterval;                                                                    // 加液和抽液之间的时间间隔(ms)
    QAtomicInt m_shouldStop{0};                                                             // 停止标志，用于中断长时间操作
    
    bool m_waitingForInput{false};                                                          // 是否正在等待用户输入
    QString m_userInput;                                                                    // 存储用户输入


};

#endif // ULAB_H
