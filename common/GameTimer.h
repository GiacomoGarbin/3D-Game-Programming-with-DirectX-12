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
};

#endif // GAME_TIMER_H