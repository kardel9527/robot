#ifndef __ROBOT_H_
#define __ROBOT_H_

class Proactor;
struct lua_State;

class Robot {
public:
	Robot() : _proactor(0), _lvm(0) {}
	Robot(const Robot &other) = delete;
	Robot& operator = (const Robot &other) = delete;
	~Robot() {}

	static Robot* instance() {
		static Robot obj;
		return &obj;
	}

	int init();
	void uninit();

	void run();
	void stop();

	Proactor* proactor() const { return _proactor;  }
	lua_State* lvm() const { return _lvm;  }

private:
	Proactor *_proactor;
	lua_State *_lvm;
};

#endif __ROBOT_H_
