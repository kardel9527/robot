#include "../luawrapper.h"
#include "../robot.h"
#include "../timeutil.h"
#include "packet.h"
#include "handle.h"

#define PING_MSG_ID 1

Handle::Handle() {
	_socket = -1;
	_active = false;
	_send_posted = false;
	_total_ping_time = 0;
	_last_ping_send_time = 0;
	_ping_times = 0;
	_total_send_bytes = 0;
	_total_recv_bytes = 0;
	_begin_ms = 0;
}

Handle::~Handle() {
	if (_socket > 0) {
		closesocket(_socket);
		_socket = -1;
	}

	for (PacketList::iterator it = _recved_packet.begin(); it != _recved_packet.end(); ++it) {
		char *packet = *it;
		free(packet);
	}

	_send_cache.clear();
	_recved_packet.clear();
	_recv_helper.release();
}

int Handle::prerecv() {
	WSABUF buf;
	buf.buf = _recv_helper.recv_ptr();
	buf.len = _recv_helper.recv_len();
	if (_recv_helper._recved_len == 0 && buf.len < 4) {
		assert(false);
	}

	ZeroMemory(&_recv_overlapped, sizeof(_recv_overlapped));
	_recv_overlapped._op = OT_RECV_POSTED;
	_recv_overlapped._handle = this;

	int ret = WSARecv(handle(), &buf, 1, (LPDWORD)&_recv_overlapped._io_size, (LPDWORD)&_recv_overlapped._flag, &_recv_overlapped, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		// failed.
		int err = WSAGetLastError();
		return -1;
	}

	return 0;
}

int Handle::presend() {
	WSABUF buf;
	_send_mutex.lock();
	if (_send_posted || _send_cache.empty()) {
		_send_mutex.unlock();
		return 0;
	}
	buf.buf = &_send_cache[0];
	buf.len = _send_cache.size();
	_send_mutex.unlock();

	ZeroMemory(&_send_overlapped, sizeof(_send_overlapped));
	_send_overlapped._op = OT_SEND_POSTED;
	_send_overlapped._handle = this;

	_send_posted = true;
	int ret = WSASend(handle(), &buf, 1, (LPDWORD)&_send_overlapped._io_size, _send_overlapped._flag, &_send_overlapped, NULL);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		// error
		int err = WSAGetLastError();
		_send_posted = false;
		return -1;
	}

	return 0;
}

void Handle::recv_complete(size_t sz) {
	if (_recv_helper.complete(sz)) {
		_recv_mutex.lock();
		_recved_packet.push_back(_recv_helper._recv_ptr);
		_recv_mutex.unlock();
		_recv_helper.reset();
	} else {
		if (_recv_helper.bad_packet()) {
			active(false);
		}
	}
}

void Handle::send_complete(size_t sz) {
	_send_mutex.lock();
	assert(_send_posted);
	if (_send_posted) {
		_send_cache.erase(_send_cache.begin(), _send_cache.begin() + sz);
		_send_posted = false;
	}
	_send_mutex.unlock();
}

void Handle::send(short op, const char *data, size_t len) {
	_total_send_bytes += len + sizeof(MsgHead);

	_send_mutex.lock();

	if (_send_cache.size() >= 10000000) {
		_send_mutex.unlock();
		return;
		//active(false);
	}

	MsgHead head;
	head._size = sizeof(MsgHead) + len;
	head._op = op;
	_send_cache.append((const char *)&head, sizeof(MsgHead));
	if (data && len) {
		_send_cache.append(data, len);
	}
	_send_mutex.unlock();
}

void Handle::kick() {
	active(false);
	shutdown(handle(), SD_RECEIVE);
}

void Handle::active(bool act) {
	_active = act;
	if (act) {
		_begin_ms = ms_proccess();
	}
}

void Handle::update() {
	if (presend()) active(false);

	update_ping();

	while (true) {
		char *packet[64] = { 0 };
		size_t len = 0;
		_recv_mutex.lock();
		len = _recved_packet.size();
		len = len > 64 ? 64 : len;
		if (!len) {
			_recv_mutex.unlock();
			return;
		}
		memcpy(packet, &_recved_packet[0], len * sizeof(char *));
		_recved_packet.erase(_recved_packet.begin(), _recved_packet.begin() + len);
		_recv_mutex.unlock();

		for (size_t i = 0; i < len; ++i) {
			// todo: call the on msg function
			MsgHead *msg = (MsgHead *)packet[i];
			_total_recv_bytes += msg->_size;

			if (msg->_op == PING_MSG_ID) {
				ping_back();
			} else {
				luawrapper::call<void>(Robot::instance()->lvm(), "onNetPacket", this, msg);
			}
			free((char *)msg);
		}
	}
}

double Handle::avg_send_flow() const {
	uint64_t scale = ms_proccess() - _begin_ms;
	return scale ? ((double)_total_send_bytes / scale) : 0;
}

double Handle::avg_recv_flow() const {
	uint64_t scale = ms_proccess() - _begin_ms;
	return scale ? ((double)_total_recv_bytes / scale) : 0;
}

void Handle::update_ping() {
	const static uint64_t s_ping_itv = 1000;
	if ((ms_proccess() - _last_ping_send_time) < s_ping_itv) return;

	_last_ping_send_time = ms_proccess();
	send(1, nullptr, 0);
}

void Handle::ping_back() {
	_ping_times++;
	_total_ping_time += ms_proccess() - _last_ping_send_time;
}