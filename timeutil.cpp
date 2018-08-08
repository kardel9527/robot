#include <time.h>
#include <windows.h>
#include "timeutil.h"

int64_t now() {
	return time(0);
}

int64_t ms_now() {
	int64_t ret = time(0);

	SYSTEMTIME st;
	GetLocalTime(&st);
	return ret * 1000 + st.wMilliseconds;
}

unsigned long long ms_proccess() {
	return GetTickCount64();
}