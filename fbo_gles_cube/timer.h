/*
 * This proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007, 2009-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdio.h>

#if defined(_WIN32)
#else
#include <sys/time.h>
#endif

/**
	Provides a platform independent high resolution timer.
	Note that the timer measures real time, not CPU time.
*/
class Timer
{
private:
	int frameCount;
	float fps;
    float lastTime;
	timeval startTime;
	timeval currentTime;
	float lastIntervalTime;
	float fpsTime;
public:
	/**
		Default Constructor
	*/
	Timer();
	/**
		Resets the timer to 0.0f.
	*/
	void reset();

	/**
		Returns the time passed since object creation or since reset() was last called.
		@return Float containing the current time.
	*/
	float getTime();

	/**
		Returns the time passed since getInterval() was last called.
		If getInterval() has not been called before, it retrieves the time passed since object creation or since reset() was called.
		@return Float containing the interval.
		
	*/
	float getInterval();

	/**
		Returns the FPS (Frames Per Second).
		This function must be called once per frame.
		@return Float containing the current FPS.
		
	*/
	float getFPS();

	/**
		Tests if 'seconds' seconds are passed since the reset or this method was called.
        @param seconds number of seconds passed default is 1.0
		@return bool true if a 'seconds' seconds are passed and false otherwise.
		
	*/
    bool isTimePassed(float seconds = 1.0f);
};

#endif //_TIMER_H_
