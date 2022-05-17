#pragma once

#include <stdio.h>
#include <Windows.h>

#include "dump/headers/dump.h"
#pragma comment(lib, "lib/dump/dump")

#include "log/headers/log.h"
#pragma comment(lib, "lib/log/log")

#include "common.h"

class CProfiler
{
public:
	struct stProfile;

public:
	
	CProfiler();

	void begin(const char name[100]);
	void end(const char name[100]);

	stProfile* begin();
	stProfile* next();

	void printToFile();

	void reset();
	
private:

	stProfile* _profile;
	unsigned int _allocIndex = 0;

	HANDLE _heap;

	LARGE_INTEGER _freq;
	CLog _logger;

	int _returnIndex;

	bool _reset;

	int findIdx(const char* name);
};

struct CProfiler::stProfile {

	stProfile() {

		ZeroMemory(_name, sizeof(_name));
		ZeroMemory(&_start, sizeof(LARGE_INTEGER));

		_sum = 0;
		_max = 0;
		_min = 0x7FFFFFFFFFFFFFFF;
		_callCnt = 0;

	}

	char _name[100];
	LARGE_INTEGER _start;
	__int64 _sum;
	__int64 _max;
	__int64 _min;
	unsigned int _callCnt;

	unsigned long _threadId;
};