#ifndef __HANDLE_H_
#define __HANDLE_H_
#include <winsock2.h>
#include <mswsock.h>
#include <vector>
#include <mutex>
#include <assert.h>

class Proactor;

class Handle {
public:
	enum OpType { OT_RECV_POSTED, OT_SEND_POSTED };
	struct OverlapedData : public OVERLAPPED {
		size_t _io_size;
		size_t _flag;
		size_t _op;
		Handle *_handle;

		OverlapedData() : _io_size(0), _flag(0), _op(0), _handle(0) {}
	};
private:
	const static size_t s_max_packet = 65535;
	struct PacketReceiveHelper {
		char *_recv_ptr;
		size_t _packet_len; // the packet length
		size_t _recved_len; // received packet length

		PacketReceiveHelper() { memset(this, 0, sizeof(*this)); }
		~PacketReceiveHelper() { release();  }

		void reset() { memset(this, 0, sizeof(*this)); }

		void release() { if (_recv_ptr) free(_recv_ptr); reset(); }

		char* recv_ptr() const {
			assert(_packet_len >= _recved_len);
			return (_recved_len >= 4 && _packet_len) ? _recv_ptr +  _recved_len : (char *)&_packet_len + _recved_len;
		}

		size_t recv_len() const {
			assert(_packet_len >= _recved_len);
			return (_recved_len >= 4 && _packet_len) ? _packet_len - _recved_len : sizeof(_packet_len) - _recved_len;
		}

		bool complete(size_t sz) {
			_recved_len += sz;
			if (_recved_len == sizeof(_packet_len)) {
				// received the packet len
				assert(!_recv_ptr && "logic error!");
				if (_packet_len >= s_max_packet) {
					return false; // a big packet;
				}

				_recv_ptr = (char *)malloc(_packet_len);
				*(size_t *)(_recv_ptr) = _packet_len;
				return false;
			} else if (_recved_len == _packet_len) {
				// received all packet data;
				return true;
			} else if (_recved_len < _packet_len) {
				// not complete.
				return false;
			} else {
				// not happend.
				//assert(false && "logic error.");
				return false;
			}

			return false;
		}

		bool bad_packet() const { return _packet_len && _recved_len >= 4 && !_recv_ptr;  }
	};

public:
	Handle();
	Handle(const Handle &other) = delete;
	Handle& operator = (const Handle &other) = delete;
	~Handle();

	long long id() const { return (long long)_socket; }
	SOCKET handle() const { return _socket;  }
	void handle(SOCKET s) { _socket = s;  }

	int prerecv();
	int presend();

	void recv_complete(size_t sz);
	void send_complete(size_t sz);

	void send(short op, const char *data, size_t len);

	void kick();

	bool active() const { return _active;  }
	void active(bool act);

	void update();

	float avg_ping() const { return _ping_times ? ((float)_total_ping_time / _ping_times) : 0; }
	unsigned long long send_bytes() const { return _total_send_bytes;  }
	unsigned long long recv_bytes() const { return _total_recv_bytes;  }
	unsigned long long total_bytes() const { return send_bytes() + recv_bytes();  }
	double avg_send_flow() const;
	double avg_recv_flow() const;
	double avg_flow() const { return avg_send_flow() + avg_recv_flow();  }

private:
	void update_ping();
	void ping_back();

private:
	SOCKET _socket;
	volatile bool _active;
	volatile bool _send_posted;

	uint64_t _total_ping_time;
	uint64_t _last_ping_send_time;
	uint64_t _ping_times;

	uint64_t _total_send_bytes;
	uint64_t _total_recv_bytes;
	uint64_t _begin_ms;

	typedef std::string SendCache;
	typedef std::vector<char *> PacketList;
	std::mutex _send_mutex;
	SendCache _send_cache;
	std::mutex _recv_mutex;
	PacketList _recved_packet;
	PacketReceiveHelper _recv_helper;

	OverlapedData _recv_overlapped;
	OverlapedData _send_overlapped;
};

#endif // __HANDLE_H_
