#ifndef __PACKET_H_
#define __PACKET_H_
#include <stdint.h>

#pragma pack(push, 1)
struct MsgHead {
	unsigned _size;
	short _op;
	//int _rpc_id;
	//int64_t _timestamp;
	MsgHead() { memset(this, 0, sizeof(*this)); }

	short opcode() const { return _op;  }
	void* data() const { return (void *)((const char*)&_size + sizeof(MsgHead)); }
	unsigned len() const { return _size - sizeof(MsgHead); }
};
#pragma pack(pop)

#endif // __PACKET_H_
