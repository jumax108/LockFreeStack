#pragma once

#include "profiler/headers/profiler.h"
#pragma comment(lib, "lib/profiler/profiler")

#include "common.h"

class CProfilerTLS {

public:

	CProfilerTLS();

	void begin(const char* tag);
	void end(const char* tag);

	void printToFile();

	void reset();

private:

	int _profilerAllocIndex;
	int _tlsIndex;
	CProfiler _profiler[profilerTLS::THREAD_NUM];
	LARGE_INTEGER _freq;

};