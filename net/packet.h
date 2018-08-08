#ifndef __PACKET_H_
#define __PACKET_H_
#include <stdint.h>

#pragma pack(push, 1)
struct MsgHead {
	unsigned _size;
	short _op;
	int _rpc_id;
	int64_t _timestamp;
	MsgHead() { memset(this, 0, sizeof(*this)); }

	const char* data() const { return (const char*)this + sizeof(*this); }
	unsigned len() const { return _size - sizeof(*this); }
};

#pragma pack(pop)

#endif // __PACKET_H_
