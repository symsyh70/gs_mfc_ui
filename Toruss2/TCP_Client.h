#pragma once

#include <functional>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afxstr.h>

#ifndef MAX_RECV_DATA
#define MAX_RECV_DATA 4096
#endif

struct COMMAND_DATA
{
    int  nLength;
    BYTE data[MAX_RECV_DATA];

    COMMAND_DATA()
    {
        nLength = 0;
        ZeroMemory(data, sizeof(data));
    }
};

class TCP_Client
{
public:
    TCP_Client(void);
    virtual ~TCP_Client(void);

    void Init();
    BOOL InitWinSock();

    BOOL Create();
    void ClearSocket();
    BOOL SetSocketOption();

    BOOL Connect(const char* lpAddress, USHORT unPort, int& nErr, int nMode = 0);

    void SetCallback(void(*p_func)(COMMAND_DATA& pData), void* ret_ptr = nullptr);
    void SetCallbackPath(void(*p_func)(CString strPath), void* ret_ptr = nullptr);

    // ±‚¡∏ raw send
    BOOL SendPacket(UINT uCommand, char* pData, UINT unSize);

    // CCB 7byte packet send
    BOOL SendCCBPacket(BYTE id, BYTE cmd1, BYTE cmd2, BYTE data1, BYTE data2);

    void SetRecvTimeout(UINT unTimeout);
    void SetSendTimeout(UINT unTimeout);

    BOOL StartReceiveThread();
    void StopReceiveThread();

    static unsigned int __stdcall RecvSockThread(void* pParam);
    void SockRecv();

    void setRecvCallback(std::function<void(const BYTE*, int)> f);

    BOOL IsConnected() const { return m_bConnected; }
    void SetConnected(BOOL bConnected) { m_bConnected = bConnected; }

private:
    using callbackRecvData = std::function<void(const BYTE*, int)>;

private:
    SOCKET m_hSocket;
    BOOL   m_bConnected;

    HANDLE m_hSockRecvThread;
    BOOL   m_bExitSockRecvThread;

    char   m_sBuffer[4096];

    void(*m_pCallbackfunc)(COMMAND_DATA& pData);
    void(*m_pCallbackPath)(CString strPath);

    callbackRecvData m_cbRecvData;
};