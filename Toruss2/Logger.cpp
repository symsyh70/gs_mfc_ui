#include "pch.h"
#include "Logger.h"

CLogger& CLogger::GetInstance()
{
    static CLogger instance;
    return instance;
}

CLogger::CLogger()
{
}

CLogger::~CLogger()
{
    if (m_file.m_pStream != NULL)
        m_file.Close();
}

void CLogger::Init(CString folder)
{
    m_folder = folder;

    if (GetFileAttributes(folder) == INVALID_FILE_ATTRIBUTES)
        CreateDirectory(folder, NULL);

    CheckDate();
}

void CLogger::CheckDate()
{
    CTime now = CTime::GetCurrentTime();

    CString date;
    date.Format(_T("%04d%02d%02d"), now.GetYear(), now.GetMonth(), now.GetDay());

    if (date != m_currentDate)
    {
        if (m_file.m_pStream != NULL)
            m_file.Close();

        CString path;
        path.Format(_T("%s\\Log_%s.txt"), m_folder, date);

        m_file.Open(path, CFile::modeCreate | CFile::modeNoTruncate | CFile::modeWrite);

        m_file.SeekToEnd();

        m_currentDate = date;
    }
}

void CLogger::WriteLog(CString level, CString message)
{
    m_lock.Lock();

    CheckDate();

    CTime now = CTime::GetCurrentTime();

    CString log;
    log.Format(_T("[%02d:%02d:%02d] [%s] %s\r\n"),
        now.GetHour(),
        now.GetMinute(),
        now.GetSecond(),
        level,
        message);

    m_file.WriteString(log);
    m_file.Flush();

    m_lock.Unlock();
}