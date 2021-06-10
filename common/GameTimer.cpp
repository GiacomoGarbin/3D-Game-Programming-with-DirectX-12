#include <windows.h>

#include "GameTimer.h"

GameTimer::GameTimer() :
    mSecondsPerCount(0.0),
    mDeltaTime(-1.0),
    mBaseTime(0),
    mPauseTime(0),
    mStopTime(0),
    mPrevTime(0),
    mCurrTime(0),
    mStopped(false)
{
    LARGE_INTEGER CountsPerSecond;
    QueryPerformanceFrequency(&CountsPerSecond);

    mSecondsPerCount = 1.0 / static_cast<double>(CountsPerSecond.QuadPart);
}

GameTimer::~GameTimer()
{
    
}

void GameTimer::reset()
{
    LARGE_INTEGER CurrTime;
    QueryPerformanceCounter(&CurrTime);

    mBaseTime = static_cast<__int64>(CurrTime.QuadPart);
    mPrevTime = static_cast<__int64>(CurrTime.QuadPart);
    mStopTime = 0;
    mStopped  = false;
}

void GameTimer::start()
{
    LARGE_INTEGER StartTime;
    QueryPerformanceCounter(&StartTime);

    if (mStopped)
    {
        __int64 StartTime64 = static_cast<__int64>(StartTime.QuadPart);

        mPauseTime += (StartTime64 - mStopTime);

        mPrevTime = StartTime64;
        mStopTime = 0;
        mStopped = false;
    }
}

void GameTimer::stop()
{
    if (!mStopped)
    {
        LARGE_INTEGER CurrTime;
        QueryPerformanceCounter(&CurrTime);

        mStopTime = static_cast<__int64>(CurrTime.QuadPart);
        mStopped = true;
    }
}

void GameTimer::tick()
{
    if (mStopped)
    {
        mDeltaTime = 0.0;
        return;
    }

    LARGE_INTEGER CurrTime;
    QueryPerformanceCounter(&CurrTime);

    mCurrTime = static_cast<__int64>(CurrTime.QuadPart);

    mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;

    mPrevTime = mCurrTime;

    if (mDeltaTime < 0.0)
    {
        mDeltaTime = 0.0;
    }
}

float GameTimer::GetDeltaTime() const
{
    return static_cast<float>(mDeltaTime);
}

float GameTimer::GetTotalTime() const
{
    if (mStopped)
    {
        return static_cast<float>(((mStopTime - mPauseTime) - mBaseTime) * mSecondsPerCount);
    }
    else
    {
        return static_cast<float>(((mCurrTime - mPauseTime) - mBaseTime) * mSecondsPerCount);
    }
}