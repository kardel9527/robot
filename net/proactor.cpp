#include "proactor.h"
#include "../luawrapper.h"
#include "../robot.h"
#include "handle.h"

void Proactor::io_routin(Proactor *proactor) {
	while (proactor->active()) {
		DWORD io_sz = 0;
		Handle::OverlapedData *overlap;
		Handle *h = 0;

		BOOL ret = GetQueuedCompletionStatus(proactor->_completion_handle, &io_sz, (LPDWORD)&h, (LPOVERLAPPED *)&overlap, INFINITE);
		if (!ret) {
			if (GetLastError() == ERROR_NETNAME_DELETED && h != NULL) {
				h->active(false);
			}
			continue;
		}

		if (!h) break;

		if (!io_sz) h->active(false);

		switch (overlap->_op) {
		case Handle::OT_RECV_POSTED:
			h->recv_complete(io_sz);
			if (h->prerecv()) h->active(false);
			break;
		case Handle::OT_SEND_POSTED:
			h->send_complete(io_sz);
			break;
		default:
			assert(false && "logic error!");
			break;
		}
	}
}

int Proactor::open() {
	if (active()) return 0;

	_completion_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (_completion_handle <= 0) return -1;

	// create io worker 
	active(true);
	for (int i = 0; i < 8; ++i) {
		_worker.push_back(new std::thread(&Proactor::io_routin, this));
	}

	return 0;
}

void Proactor::close() {
	if (!active()) return;

	active(false);
	CloseHandle(_completion_handle);
	_completion_handle = INVALID_HANDLE_VALUE;

	for (size_t i = 0; i < _worker.size(); ++i) {
		_worker[i]->join();
		delete _worker[i];
	}

	_worker.clear();
}

Handle* Proactor::connect(const char *ip, short port) {
	SOCKET s = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	
	SOCKADDR_IN addr;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(port);

	// 
	int ret = ::connect(s, (SOCKADDR *)&addr, sizeof(SOCKADDR_IN));
	if (ret == SOCKET_ERROR) {
		closesocket(s);
		return nullptr;
	}

	Handle *h = new Handle();

	h->handle(s);
	CreateIoCompletionPort((HANDLE)s, _completion_handle, (ULONG_PTR)h, 0);
	h->active(true);
	_active_handle[s] = h;

	return h;
}

void Proactor::update() {
	for (auto it = _active_handle.begin(); it != _active_handle.end(); ) {
		Handle *h = it->second;
		h->update();
		if (!h->active()) {
			luawrapper::call<void>(Robot::instance()->lvm(), "onDisconnected", h->id());
			it = _active_handle.erase(it);
			delete h;
		} else {
			++it;
		}
	}
}