#include "pch.h"
#include "TCP_Client.h"

#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <process.h>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

void ErrorMessage()
{
    LPVOID lpMsgBuf = NULL;
    DWORD dwErroCode = WSAGetLastError();

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dwErroCode,
        0,
        (LPWSTR)&lpMsgBuf,
        0,
        NULL);

    TRACE(_T("ERROR MESSAGE(%lu) : %s\n"), dwErroCode, (LPWSTR)lpMsgBuf);

    LocalFree(lpMsgBuf);
}

static BYTE CheckSum(BYTE* data, int len)
{
    BYTE sum = 0;
    for (int i = 0; i < len; i++)
        sum += data[i];

    return (sum & 0xFF);
}

TCP_Client::TCP_Client(void)
{
    m_cbRecvData = nullptr;

    m_hSockRecvThread = NULL;
    m_bExitSockRecvThread = FALSE;

    m_hSocket = INVALID_SOCKET;
    m_bConnected = FALSE;

    m_pCallbackfunc = nullptr;
    m_pCallbackPath = nullptr;

    ZeroMemory(m_sBuffer, sizeof(m_sBuffer));

    Init();
}

TCP_Client::~TCP_Client(void)
{
    StopReceiveThread();
    ClearSocket();
    WSACleanup();
}

void TCP_Client::Init()
{
    SetConnected(FALSE);
    m_hSocket = INVALID_SOCKET;
    InitWinSock();
}

BOOL TCP_Client::InitWinSock()
{
    WSADATA WsaData;

    if (WSAStartup(MAKEWORD(2, 2), &WsaData) == SOCKET_ERROR)
        return FALSE;

    return TRUE;
}

BOOL TCP_Client::Create()
{
    m_hSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (m_hSocket == INVALID_SOCKET)
    {
        TRACE(_T("Socket : INVALID SOCKET\n"));
        return FALSE;
    }

    return TRUE;
}

void TCP_Client::ClearSocket()
{
    if (m_hSocket != INVALID_SOCKET)
    {
        LINGER lingerStruct;
        lingerStruct.l_onoff = 1;
        lingerStruct.l_linger = 0;

        setsockopt(m_hSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerStruct, sizeof(lingerStruct));

        shutdown(m_hSocket, SD_BOTH);
        closesocket(m_hSocket);

        m_hSocket = INVALID_SOCKET;
    }

    SetConnected(FALSE);
}

BOOL TCP_Client::SetSocketOption()
{
    BOOL bSockOpt = TRUE;

    if (setsockopt(m_hSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&bSockOpt, sizeof(bSockOpt)) == SOCKET_ERROR)
    {
        ClearSocket();
        return FALSE;
    }

    return TRUE;
}

BOOL TCP_Client::Connect(const char* lpAddress, USHORT unPort, int& nErr, int nMode)
{
    UNREFERENCED_PARAMETER(nMode);

    nErr = 0;

    if (m_hSocket == INVALID_SOCKET)
    {
        nErr = WSAENOTSOCK;
        return FALSE;
    }

    char szPort[16] = { 0 };
    sprintf_s(szPort, "%u", (unsigned int)unPort);

    addrinfo hints = {};
    addrinfo* result = nullptr;
    addrinfo* ptr = nullptr;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int ret = getaddrinfo(lpAddress, szPort, &hints, &result);
    if (ret != 0)
    {
        nErr = ret;
        return FALSE;
    }

    BOOL bConnected = FALSE;

    for (ptr = result; ptr != nullptr; ptr = ptr->ai_next)
    {
        if (::connect(m_hSocket, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR)
        {
            nErr = WSAGetLastError();
            continue;
        }

        bConnected = TRUE;
        break;
    }

    freeaddrinfo(result);

    if (!bConnected)
        return FALSE;

    SetConnected(TRUE);
    StartReceiveThread();

    return TRUE;
}

void TCP_Client::SetCallback(void(*p_func)(COMMAND_DATA& pData), void* ret_ptr)
{
    UNREFERENCED_PARAMETER(ret_ptr);
    m_pCallbackfunc = p_func;
}

void TCP_Client::SetCallbackPath(void(*p_func)(CString strPath), void* ret_ptr)
{
    UNREFERENCED_PARAMETER(ret_ptr);
    m_pCallbackPath = p_func;
}

BOOL TCP_Client::SendPacket(UINT uCommand, char* pData, UINT unSize)
{
    UNREFERENCED_PARAMETER(uCommand);

    if (m_hSocket == INVALID_SOCKET || !m_bConnected)
        return FALSE;

    int nRet = send(m_hSocket, pData, (int)unSize, 0);

    if (nRet == SOCKET_ERROR)
    {
        ErrorMessage();
        return FALSE;
    }

    return TRUE;
}

BOOL TCP_Client::SendCCBPacket(BYTE id, BYTE cmd1, BYTE cmd2, BYTE data1, BYTE data2)
{
    if (m_hSocket == INVALID_SOCKET || !m_bConnected)
        return FALSE;

    BYTE packet[7] = { 0 };

    packet[0] = 0xFF;
    packet[1] = id;
    packet[2] = cmd1;
    packet[3] = cmd2;
    packet[4] = data1;
    packet[5] = data2;
    packet[6] = CheckSum(&packet[1], 5);   // Byte2 ~ Byte6

    int nRet = send(m_hSocket, (const char*)packet, 7, 0);

    if (nRet == SOCKET_ERROR)
    {
        ErrorMessage();
        return FALSE;
    }

    printf("[SEND] ");
    for (int i = 0; i < 7; i++)
        printf("%02X ", packet[i]);
    printf("\n");

    return TRUE;
}

void TCP_Client::SetRecvTimeout(UINT unTimeout)
{
    if (setsockopt(m_hSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&unTimeout, sizeof(unTimeout)) == SOCKET_ERROR)
    {
        ClearSocket();
    }
}

void TCP_Client::SetSendTimeout(UINT unTimeout)
{
    if (setsockopt(m_hSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&unTimeout, sizeof(unTimeout)) == SOCKET_ERROR)
    {
        ClearSocket();
    }
}

BOOL TCP_Client::StartReceiveThread()
{
    if (m_hSockRecvThread == NULL)
    {
        m_bExitSockRecvThread = FALSE;
        m_hSockRecvThread = (HANDLE)_beginthreadex(NULL, 0, RecvSockThread, this, 0, NULL);
    }

    return TRUE;
}

void TCP_Client::StopReceiveThread()
{
    if (m_hSockRecvThread)
    {
        m_bExitSockRecvThread = TRUE;
        WaitForSingleObject((HANDLE)m_hSockRecvThread, 1000);
        CloseHandle((HANDLE)m_hSockRecvThread);

        m_hSockRecvThread = NULL;
        m_bExitSockRecvThread = FALSE;
    }
}

void TCP_Client::setRecvCallback(callbackRecvData f)
{
    m_cbRecvData = std::move(f);
}

unsigned int __stdcall TCP_Client::RecvSockThread(void* pParam)
{
    TCP_Client* pSocket = (TCP_Client*)pParam;
    if (pSocket)
        pSocket->SockRecv();

    return 1;
}

void TCP_Client::SockRecv()
{
    int nRecvBytes = 0;

    while (m_bExitSockRecvThread == FALSE &&
        m_hSocket != INVALID_SOCKET)
    {
        nRecvBytes = recv(m_hSocket, m_sBuffer, sizeof(m_sBuffer), 0);

        if (nRecvBytes > 0)
        {
            printf("[RECV] ");
            for (int i = 0; i < nRecvBytes; i++)
                printf("%02X ", (BYTE)m_sBuffer[i]);
            printf("\n");

            if (m_cbRecvData)
            {
                m_cbRecvData((const BYTE*)m_sBuffer, nRecvBytes);
            }

            if (m_pCallbackfunc)
            {
                COMMAND_DATA stData;
                stData.nLength = nRecvBytes;
                memcpy(stData.data, m_sBuffer, nRecvBytes);
                m_pCallbackfunc(stData);
            }
        }
        else if (nRecvBytes == 0)
        {
            printf("[RECV] disconnected\n");
            break;
        }
        else
        {
            int err = WSAGetLastError();

            if (err == WSAEWOULDBLOCK)
            {
                Sleep(5);
                continue;
            }

            printf("[RECV] error = %d\n", err);
            break;
        }

        Sleep(5);
    }

    SetConnected(FALSE);
}