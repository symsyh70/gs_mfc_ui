#pragma once
#include <mutex>
#include <condition_variable>

// PTZ 명령 타입
enum class PTZCmdType { Move, Stop };

// PTZ 명령 구조체
struct PTZCommand
{
    PTZCmdType type = PTZCmdType::Stop;
    BYTE       pan = 0x00;   // 보통 0x00 고정 (cmd는 tilt 바이트에)
    BYTE       tilt = 0x00;   // direction cmd byte (0x02/0x04/0x08/0x10 등)
    BYTE       panSpeed = 0x00;
    BYTE       tiltSpeed = 0x00;
};

// Size-1 슬롯: 새 명령이 오면 이전 명령을 덮어씀 → 누적 없음, 항상 최신 명령만 전송
class PTZCommandSlot
{
public:
    // 생산자(TrackLoop/AIThread): 덮어쓰기 후 즉시 반환
    void Post(const PTZCommand& cmd)
    {
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            m_slot = cmd;
            m_hasNew = true;
        }
        m_cv.notify_one();
    }

    // 소비자(DispatchThread): 새 명령 올 때까지 대기 후 반환
    PTZCommand WaitAndTake()
    {
        std::unique_lock<std::mutex> lk(m_mtx);
        m_cv.wait(lk, [this] { return m_hasNew || m_stop; });
        m_hasNew = false;
        return m_slot;
    }

    // 종료 요청
    void RequestStop()
    {
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            m_stop = true;
        }
        m_cv.notify_all();
    }

    bool IsStopping() const { return m_stop; }

private:
    std::mutex              m_mtx;
    std::condition_variable m_cv;
    PTZCommand              m_slot;
    bool                    m_hasNew = false;
    bool                    m_stop = false;
};