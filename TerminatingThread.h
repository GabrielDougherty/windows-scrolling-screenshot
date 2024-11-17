#pragma once

#include <thread>
#include <utility>

class TerminatingThread
{
private:
	std::jthread innerThread;
public:
	template<class T>
	TerminatingThread(T&& func) : innerThread(std::forward<T>(func))
	{
	}
	~TerminatingThread()
	{
		innerThread.request_stop();
	}
};