#ifndef __PROACTOR_H_
#define __PROACTOR_H_
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <mswsock.h>
#include <map>
#include <vector>
#include <thread>

class Handle;

class Proactor {
public:
	Proactor() : _active(false), _completion_handle(INVALID_HANDLE_VALUE) {}
	Proactor(const Proactor &other) = delete;
	Proactor& operator = (const Proactor &other) = delete;
	~Proactor() {}

	bool active() const { return _active;  }
	void active(bool act) { _active = act;  }

	int open();
	void close();

	Handle* connect(const char *ip, short port);

	void update();

private:
	static void io_routin(Proactor *proactor);

private:
	volatile bool _active;
	HANDLE _completion_handle;
	std::vector<std::thread *> _worker;

	typedef std::map<SOCKET, Handle *> HandleMap;
	HandleMap _active_handle;
};

#endif // __PROACTOR_H_
