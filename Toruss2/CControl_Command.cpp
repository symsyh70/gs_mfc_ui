#include "pch.h"
#include "CControl_Command.h"

CControl_Command::CControl_Command()
    : m_nUnitID(0x01)
{
}

CControl_Command::~CControl_Command()
{
    DisconnectCCB();
}

BOOL CControl_Command::ConnectCCB(const CString& strIP, int nPort)
{
    DisconnectCCB();

    int nErr = 0;

    m_tcp.Init();

    if (!m_tcp.Create())
        return FALSE;

    if (!m_tcp.SetSocketOption())
        return FALSE;

#ifdef UNICODE
    CStringA ipA(strIP);
    const char* pszIP = ipA.GetString();
#else
    const char* pszIP = strIP;
#endif

    if (!m_tcp.Connect(pszIP, (USHORT)nPort, nErr, 0))
    {
        TRACE(_T("ConnectCCB Fail : %d\n"), nErr);
        return FALSE;
    }

    return TRUE;
}

void CControl_Command::DisconnectCCB()
{
    m_tcp.StopReceiveThread();
    m_tcp.ClearSocket();
}

BOOL CControl_Command::IsConnected() const
{
    return m_tcp.IsConnected();
}

void CControl_Command::SetUnitID(BYTE id)
{
    m_nUnitID = id;
}

void CControl_Command::SetRecvCallback(void(*p_func)(COMMAND_DATA& pData), void* pUser)
{
    m_tcp.SetCallback(p_func, pUser);
}

BOOL CControl_Command::SendCCB(BYTE cmd1, BYTE cmd2, BYTE data1, BYTE data2)
{
    return m_tcp.SendCCBPacket(m_nUnitID, cmd1, cmd2, data1, data2);
}

BOOL CControl_Command::MoveLeft(BYTE speed)
{
    return SendCCB(0x00, 0x04, speed, speed);
}

BOOL CControl_Command::MoveRight(BYTE speed)
{
    return SendCCB(0x00, 0x02, speed, speed);
}

BOOL CControl_Command::MoveUp(BYTE speed)
{
    return SendCCB(0x00, 0x08, speed, speed);
}

BOOL CControl_Command::MoveDown(BYTE speed)
{
    return SendCCB(0x00, 0x10, speed, speed);
}

BOOL CControl_Command::MoveLeftUp(BYTE panSpeed, BYTE tiltSpeed)
{
    return SendCCB(0x00, 0x0C, panSpeed, tiltSpeed);
}

BOOL CControl_Command::MoveRightUp(BYTE panSpeed, BYTE tiltSpeed)
{
    return SendCCB(0x00, 0x0A, panSpeed, tiltSpeed);
}

BOOL CControl_Command::MoveLeftDown(BYTE panSpeed, BYTE tiltSpeed)
{
    return SendCCB(0x00, 0x14, panSpeed, tiltSpeed);
}

BOOL CControl_Command::MoveRightDown(BYTE panSpeed, BYTE tiltSpeed)
{
    return SendCCB(0x00, 0x12, panSpeed, tiltSpeed);
}

BOOL CControl_Command::Stop()
{
    return SendCCB(0x00, 0x00, 0x00, 0x00);
}

BOOL CControl_Command::ZoomIn(BYTE speed)
{
    return SendCCB(0x01, 0x00, speed, 0x00);
}

BOOL CControl_Command::ZoomOut(BYTE speed)
{
    return SendCCB(0x02, 0x00, speed, 0x00);
}

BOOL CControl_Command::FocusNear(BYTE speed)
{
    return SendCCB(0x03, 0x00, speed, 0x00);
}

BOOL CControl_Command::FocusFar(BYTE speed)
{
    return SendCCB(0x04, 0x00, speed, 0x00);
}

TCP_Client* CControl_Command::GetClient()
{
    return &m_tcp;
}

BOOL CControl_Command::SetPanSpeed(BYTE speed)
{
    SHORT nScan_Speed = 5;
    nScan_Speed = SHORT(speed * 100.0);

    BYTE D1 = nScan_Speed >> 8;
    BYTE D2 = nScan_Speed;

    return SendCCB(0.00, 0x49, D1, D2);
}

BOOL CControl_Command::SetTiltSpeed(BYTE speed)
{
    SHORT nScan_Speed = 5;
    nScan_Speed = SHORT(speed * 100.0);

    BYTE D1 = nScan_Speed >> 8;
    BYTE D2 = nScan_Speed;

    return SendCCB(0.00, 0x4B, D1, D2);
}

short DegToProtocol(double deg)
{
    return (short)(deg * 100.0);
}

BOOL CControl_Command::PanGoPosition(double deg)
{

    while (deg > 180.0)		deg -= 360.0;
    while (deg < -180.0)	deg += 360.0;

    SHORT DegX100 = 0;
    if (deg < 0)
        DegX100 = (deg - 0.005) * 100;
    else
        DegX100 = (deg + 0.005) * 100;

    BYTE D1 = DegX100 >> 8;
    BYTE D2 = DegX100;
    return SendCCB(0x00, 0x45, D1, D2);		// 0x41 사용시 Max Speed
}

BOOL CControl_Command::TiltGoPosition(double deg)
{
    while (deg > 180.0)		deg -= 360.0;
    while (deg < -180.0)	deg += 360.0;

    SHORT DegX100 = 0;
    if (deg < 0)
        DegX100 = (deg - 0.005) * 100;
    else
        DegX100 = (deg + 0.005) * 100;

    BYTE D1 = DegX100 >> 8;
    BYTE D2 = DegX100;
    return SendCCB(0x00, 0x47, D1, D2);		// 0x41 사용시 Max Speed
}