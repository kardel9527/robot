#include <assert.h>
#include "luawrapper.h"
#include "net/handle.h"
#include "net/proactor.h"
#include "net/packet.h"
#include "timeutil.h"
#include "md5.h"
#include "robot.h"

#define FRAME_ITV 30 // 

static const char* lua_md5(const char *src) {
	static char buff[33] = { 0 };
	md5_make(src, buff, sizeof(buff));
	return buff;
}

int Robot::init() {
	_proactor = new Proactor();
	_lvm = luawrapper::init();
	assert(_proactor && _lvm);

	if (_proactor->open()) {
		uninit();
		return -1;
	}

	// register lua api
	// proactor interface 
	luawrapper::object_reg<Proactor>(_lvm, "Proactor");
	luawrapper::object_method_reg<Proactor>(_lvm, "connect", &Proactor::connect);

	// connection interface
	luawrapper::object_reg<Handle>(_lvm, "Handle");
	luawrapper::object_method_reg<Handle>(_lvm, "id", &Handle::id);
	luawrapper::object_method_reg<Handle>(_lvm, "send", &Handle::send);
	luawrapper::object_method_reg<Handle>(_lvm, "kick", &Handle::kick);
	luawrapper::object_method_reg<Handle>(_lvm, "avg_ping", &Handle::avg_ping);
	luawrapper::object_method_reg<Handle>(_lvm, "send_bytes", &Handle::send_bytes);
	luawrapper::object_method_reg<Handle>(_lvm, "recv_bytes", &Handle::recv_bytes);
	luawrapper::object_method_reg<Handle>(_lvm, "total_bytes", &Handle::total_bytes);
	luawrapper::object_method_reg<Handle>(_lvm, "avg_send_flow", &Handle::avg_send_flow);
	luawrapper::object_method_reg<Handle>(_lvm, "avg_recv_flow", &Handle::avg_recv_flow);
	luawrapper::object_method_reg<Handle>(_lvm, "avg_flow", &Handle::avg_flow);

	// msg interface
	luawrapper::object_reg<MsgHead>(_lvm, "MsgHead");
	luawrapper::object_method_reg<MsgHead>(_lvm, "opcode", &MsgHead::opcode);
	luawrapper::object_method_reg<MsgHead>(_lvm, "data", &MsgHead::data);
	luawrapper::object_method_reg<MsgHead>(_lvm, "len", &MsgHead::len);

	// robot interface
	luawrapper::object_reg<Robot>(_lvm, "Robot");
	luawrapper::object_method_reg<Robot>(_lvm, "proactor", &Robot::proactor);

	// register the robot instance to lua
	luawrapper::set(_lvm, "s_robot", Robot::instance());

	// register global function
	luawrapper::def(_lvm, "ms_proccess", ms_proccess);

	// register md5 function
	luawrapper::def(_lvm, "md5", lua_md5);

	// do the init lua script
	luawrapper::dofile(_lvm, "./script/load.lua");
	
	// invoke the start script function
	luawrapper::call<void>(_lvm, "onStart");

	return 0;
}

void Robot::uninit() {
	if (_proactor) {
		delete _proactor;
		_proactor = nullptr;
	}

	if (_lvm) {
		luawrapper::uninit(_lvm);
		_lvm = nullptr;
	}
}

void Robot::run() {
	while (_proactor->active()) {
		int64_t t_begin = ms_now();
		_proactor->update();
		luawrapper::call<void>(_lvm, "update");
		int64_t t_end = ms_now();

		if ((t_end - t_begin) < FRAME_ITV)
		{
			unsigned long delay = FRAME_ITV - (t_end - t_begin);
			Sleep(delay);
		}

	}
}

void Robot::stop() {
	_proactor->close();
}
