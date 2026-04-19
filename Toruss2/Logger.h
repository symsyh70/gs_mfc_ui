#pragma once
#include <afx.h>
#include <afxmt.h>

class CLogger
{
public:
    static CLogger& GetInstance();

    void Init(CString folder);
    void WriteLog(CString level, CString message);

private:
    CLogger();
    ~CLogger();

    CString m_folder;
    CString m_currentDate;

    CStdioFile m_file;
    CCriticalSection m_lock;

    void CheckDate();
};