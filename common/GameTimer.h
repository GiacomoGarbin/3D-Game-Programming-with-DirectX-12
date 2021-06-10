#ifndef GAME_TIMER_H
#define GAME_TIMER_H

class GameTimer
{
	double mSecondsPerCount;
	double mDeltaTime;

	__int64 mBaseTime;
	__int64 mPauseTime;
	__int64 mStopTime;
	__int64 mPrevTime;
	__int64 mCurrTime;

	bool mStopped;

public:
    GameTimer();
    ~GameTimer();

	void reset();
	void start();
	void stop();
	void tick();

	float GetDeltaTime() const;
	float GetTotalTime() const;
};

#endif // GAME_TIMER_H