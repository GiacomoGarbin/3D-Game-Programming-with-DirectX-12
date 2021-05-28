#ifndef GAME_TIMER_H
#define GAME_TIMER_H

class GameTimer
{
public:
    GameTimer();
    ~GameTimer();

	void reset();
	void start();
	void stop();
	void tick();

	float GetDeltaTime();
	float GetTotalTime();
};

#endif // GAME_TIMER_H