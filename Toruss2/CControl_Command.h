#pragma once

#include "TCP_Client.h"

class CControl_Command
{
public:
    CControl_Command();
    ~CControl_Command();

public:
    BOOL ConnectCCB(const CString& strIP, int nPort);
    void DisconnectCCB();

    BOOL IsConnected() const;
    void SetUnitID(BYTE id);

    void SetRecvCallback(void(*p_func)(COMMAND_DATA& pData), void* pUser = nullptr);

    // 공통 송신
    BOOL SendCCB(BYTE cmd1, BYTE cmd2, BYTE data1, BYTE data2);

    // PTZ
    BOOL MoveLeft(BYTE speed = 0x1F);
    BOOL MoveRight(BYTE speed = 0x1F);
    BOOL MoveUp(BYTE speed = 0x1F);
    BOOL MoveDown(BYTE speed = 0x1F);

    BOOL MoveLeftUp(BYTE panSpeed = 0x1F, BYTE tiltSpeed = 0x1F);
    BOOL MoveRightUp(BYTE panSpeed = 0x1F, BYTE tiltSpeed = 0x1F);
    BOOL MoveLeftDown(BYTE panSpeed = 0x1F, BYTE tiltSpeed = 0x1F);
    BOOL MoveRightDown(BYTE panSpeed = 0x1F, BYTE tiltSpeed = 0x1F);

    BOOL Stop();

    // Zoom / Focus
    BOOL ZoomIn(BYTE speed = 0x1F);
    BOOL ZoomOut(BYTE speed = 0x1F);
    BOOL FocusNear(BYTE speed = 0x1F);
    BOOL FocusFar(BYTE speed = 0x1F);

    TCP_Client* GetClient();

    BOOL SetPanSpeed(BYTE speed);
    BOOL SetTiltSpeed(BYTE speed);

    BOOL PanGoPosition(double deg);
    BOOL TiltGoPosition(double deg);

private:
    TCP_Client m_tcp;
    BYTE m_nUnitID;
};