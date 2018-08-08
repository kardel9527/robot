#ifndef __LUA_WRAPPER_H_
#define __LUA_WRAPPER_H_
#include <new>
#include <assert.h>
#include <string.h>
#include <type_traits>
#include "lua.hpp"

namespace luawrapper {

	lua_State* init();
	void uninit(lua_State *L);
	void dofile(lua_State *L, const char *file);
	void dostring(lua_State *L, const char *str);
	void dobuffer(lua_State *L, const char *buff, unsigned sz);

	void enum_stack(lua_State *L);
	int on_error(lua_State *L);

	template<typename T>
	struct ObjectName {
		static char name[256];

		static void reg_name(const char *n) {
			if (name[0]) return;

			unsigned len = strlen(n);
			if (len > 255) return;
			memcpy(name, n, len);
		}
	};

	template<typename T> char ObjectName<T>::name[256] = { 0 };

	template<typename T> inline void unused(T &t) { }

	template<bool b, typename A, typename B> struct _if {};
	template<typename A, typename B> struct _if<true, A, B> { typedef A type; };
	template<typename A, typename B> struct _if<false, A, B> { typedef B type; };

	template<typename T>
	struct ObjectWrapper {
		ObjectWrapper() : _p(0) {}
		ObjectWrapper(T *t, bool mgr) : _p((void *)t), _mgr(mgr) {}
		~ObjectWrapper() { if (_mgr) delete ((T*)_p); }

		T get_obj() { return T(*(T*)_p); } // return a temp object
		T* get_ptr() { return (T*)_p; }
		T& get_ref() { return *(T*)_p; }

		void *_p;
		bool _mgr;
	};

	// method read an object from lua stack
	// TODO: support lua table and function?
	template<typename T> struct lua2object {
		static T read(lua_State *L, int idx) {
			assert(lua_isuserdata(L, idx) && "error type!");
			static_assert(std::is_class<T>::value || std::is_union<T>::value, "error type!");
			ObjectWrapper<T> *ptr = (ObjectWrapper<T> *)lua_touserdata(L, idx);
			return ptr->get_obj();
		}
	};

	template<typename T> struct lua2object<T *> {
		static T* read(lua_State *L, int idx) {
			assert(lua_isuserdata(L, idx) && "error type!");
			static_assert(std::is_class<typename std::remove_pointer<T>::type>::value || std::is_union<typename std::remove_pointer<T>::type>::value, "error type!");
			ObjectWrapper<T> *ptr = (ObjectWrapper<T> *)lua_touserdata(L, idx);
			return ptr->get_ptr();
		}
	};

	template<typename T> struct lua2object<T &> {
		static T& read(lua_State *L, int idx) {
			assert(lua_isuserdata(L, idx) && "error type!");
			static_assert(std::is_class<typename std::remove_reference<T>::type>::value || std::is_union<typename std::remove_reference<T>::type>::value, "error type!");
			ObjectWrapper<T> *ptr = (ObjectWrapper<T> *)lua_touserdata(L, idx);
			return ptr->get_ref();
		}
	};

	template<typename T> struct lua2enum {
		static T read(lua_State *L, int idx) {
			return (T)lua_tointeger(L, idx);
		}
	};

	template<typename T> T read(lua_State *L, int idx) { return _if<std::is_enum<T>::value, lua2enum<T>, lua2object<T> >::type::read(L, idx); }
	template<> bool read(lua_State *L, int idx);
	template<> char read(lua_State *L, int idx);
	template<> signed char read(lua_State *L, int idx);
	template<> unsigned char read(lua_State *L, int idx);
	template<> short read(lua_State *L, int idx);
	template<> unsigned short read(lua_State *L, int idx);
	template<> int read(lua_State *L, int idx);
	template<> unsigned int read(lua_State *L, int idx);
	template<> long read(lua_State *L, int idx);
	template<> unsigned long read(lua_State *L, int idx);
	template<> long long read(lua_State *L, int idx);
	template<> unsigned long long read(lua_State *L, int idx);
	template<> float read(lua_State *L, int idx);
	template<> double read(lua_State *L, int idx);
	template<> void read(lua_State *L, int idx);
	template<> const char* read(lua_State *L, int idx);

	// pop an object from lua stack and return the object
	// TODO: support lua table and function?
	template<typename T> T pop(lua_State *L) { T ret = read<T>(L, -1); lua_pop(L, 1); return ret; }
	template<> void pop(lua_State *L);

	// get the underlying type of T
	template<typename T> struct base { typedef typename std::remove_const<T>::type type; };
	template<typename T> struct base<T *> { typedef typename std::remove_pointer<T>::type type; };
	template<typename T> struct base<T &> { typedef typename std::remove_reference<T>::type type; };

	// push an object to lua stack
	// TODO: support lua table and function?
	template<typename T> struct object2lua {
		static void push(lua_State *L, T &t) {
			static_assert(std::is_class<T>::value || std::is_union<T>::value, "error type!");
			new(lua_newuserdata(L, sizeof(ObjectWrapper<T>))) ObjectWrapper<T>(new T(t), true);
			lua_getglobal(L, ObjectName<typename base<T>::type>::name);
			lua_setmetatable(L, -2);
		}
	};

	template<typename T> struct object2lua<T *> {
		static void push(lua_State *L, T *t) {
			static_assert(std::is_class<typename std::remove_pointer<T>::type>::value || std::is_union<typename std::remove_pointer<T>::type>::value, "error type!");
			//lua_pushlightuserdata(L, t);
			if (t) {
				new(lua_newuserdata(L, sizeof(ObjectWrapper<T>))) ObjectWrapper<T>(t, false);
				lua_getglobal(L, ObjectName<typename base<T>::type>::name);
				lua_setmetatable(L, -2);
			} else {
				lua_pushnil(L);
			}
		}
	};

	template<typename T> struct object2lua<T &> {
		static void push(lua_State *L, T &t) {
			static_assert(std::is_class<typename std::remove_reference<T>::type>::value || std::is_union<typename std::remove_reference<T>::type>::value, "error type!");
			//lua_pushlightuserdata(L, &t);
			new(lua_newuserdata(L, sizeof(ObjectWrapper<T>))) ObjectWrapper<T>(&t, false);
			lua_getglobal(L, ObjectName<typename base<T>::type>::name);
			lua_setmetatable(L, -2);
		}
	};

	template<typename T> struct enum2lua {
		static void push(lua_State *L, T t) { lua_pushinteger(L, (int)t); }
	};

	template<typename T> void push(lua_State *L, T t) { _if<std::is_enum<T>::value, enum2lua<T>, object2lua<T> >::type::push(L, t); }
	template<> void push(lua_State *L, bool val);
	template<> void push(lua_State *L, char val);
	template<> void push(lua_State *L, signed char val);
	template<> void push(lua_State *L, unsigned char val);
	template<> void push(lua_State *L, short val);
	template<> void push(lua_State *L, unsigned short val);
	template<> void push(lua_State *L, int val);
	template<> void push(lua_State *L, unsigned int val);
	template<> void push(lua_State *L, long val);
	template<> void push(lua_State *L, unsigned long val);
	template<> void push(lua_State *L, long long val);
	template<> void push(lua_State *L, unsigned long long val);
	template<> void push(lua_State *L, float val);
	template<> void push(lua_State *L, double val);
	template<> void push(lua_State *L, const char *val);

	// construct and destructor of a object, TODO: pooled object?.
	template<typename T, typename ... Args>
	int constructor(lua_State *L) {
		int idx = sizeof...(Args);
		//T *t = new T(read<Args>(L, idx--)...);
		//lua_pushlightuserdata(L, t);
		new(lua_newuserdata(L, sizeof(ObjectWrapper<T>))) ObjectWrapper<T>(new T(read<Args>(L, idx--)...), true);
		lua_getglobal(L, ObjectName<typename base<T>::type>::name);
		lua_setmetatable(L, -2);
		return 1;
	}

	// push a constructor to lua stack
	template<typename T, typename ... Args>
	void push_constructor(lua_State *L, void (T::*)(Args...)) {
		lua_pushcclosure(L, constructor<T, Args...>, 1);
	}

	template<typename T>
	int destructor(lua_State *L) {
		assert(lua_isuserdata(L, 1) && "error gc type");
		ObjectWrapper<T> *ptr = (ObjectWrapper<T> *)lua_touserdata(L, 1);
		ptr->~ObjectWrapper<T>();
		return 0;
	}

	// templates convert the method upvalue
	template<typename T> struct upvaluetomethod {
		static T convert(lua_State *L) {
			static_assert(std::is_function<typename std::remove_pointer<T>::type>::value, "only function supported!");
			return (T)lua_touserdata(L, lua_upvalueindex(1));
		}
	};

	template<typename T> struct upvaluetomemmethod {
		static T convert(lua_State *L) {
			static_assert(std::is_member_function_pointer<T>::value, "only function supported!");
			return *(T*)lua_touserdata(L, lua_upvalueindex(1));
		}
	};

	template<typename T> T void2method(lua_State *L) {
		return _if<std::is_member_function_pointer<T>::value, upvaluetomemmethod<T>, upvaluetomethod<T> >::type::convert(L);
	}

	// method wrapper of function and member function, it push and read parameters automatically
	template<typename RT, typename ... Args>
	int method_with_ret(lua_State *L) {
		int idx = sizeof...(Args);
		push<RT>(L, void2method<RT(*)(Args...)>(L)(read<Args>(L, idx--)...));
		return 1;
	}

	template<typename RT, typename ... Args>
	void push_method(lua_State *L, RT(*)(Args...)) {
		lua_pushcclosure(L, method_with_ret<RT, Args...>, 1);
	}

	template<typename ... Args>
	int method_without_ret(lua_State *L) {
		int idx = sizeof...(Args);
		void2method<void(*)(Args...)>(L)(read<Args>(L, idx--)...);
		return 0;
	}

	template<typename ... Args>
	void push_method(lua_State *L, void(*)(Args...)) {
		lua_pushcclosure(L, method_without_ret<Args...>, 1);
	}

	template<typename T, typename RT, typename ... Args>
	int mem_method_with_ret(lua_State *L) {
		int idx = sizeof...(Args)+1; // this
		push<RT>(L, (read<T*>(L, 1)->*void2method<RT(T::*)(Args...)>(L))(read<Args>(L, idx--)...));
		return 1;
	}

	template<typename T, typename RT, typename ... Args>
	void push_method(lua_State *L, RT(T::*)(Args...)) {
		lua_pushcclosure(L, mem_method_with_ret<T, RT, Args...>, 1);
	}

	template<typename T, typename RT, typename ... Args>
	void push_method(lua_State *L, RT(T::*)(Args...) const) {
		lua_pushcclosure(L, mem_method_with_ret<T, RT, Args...>, 1);
	}

	template<typename T, typename ... Args>
	int mem_method_without_ret(lua_State *L) {
		int idx = sizeof...(Args)+1; // this
		(read<T*>(L, 1)->*void2method<void (T::*)(Args...)>(L))(read<Args>(L, idx--)...);
		return 0;
	}

	template<typename T, typename ... Args>
	void push_method(lua_State *L, void (T::*)(Args...)) {
		lua_pushcclosure(L, mem_method_without_ret<T, Args...>, 1);
	}

	template<typename T, typename ... Args>
	void push_method(lua_State *L, void (T::*)(Args...) const) {
		lua_pushcclosure(L, mem_method_without_ret<T, Args...>, 1);
	}

	template<typename T> struct RetNum { enum { value = 1 }; };
	template<> struct RetNum<void> { enum { value = 0 }; };

	// call a global lua function
	template<typename RT, typename ... Args>
	RT call(lua_State *L, const char *func, const Args& ... args) {
		lua_pushcclosure(L, on_error, 0);
		int err_func = lua_gettop(L);

		lua_getglobal(L, func);
		if (lua_isfunction(L, -1)) {
			int arr[] = { 0, (push(L, args), 0)... };
			unused(arr); // avoid warnning
			lua_pcall(L, sizeof...(args), RetNum<RT>::value, err_func);
		}
		else {
			assert(false && "attemp to call a none function!");
		}

		lua_remove(L, err_func);

		return pop<RT>(L);
	}

	// direct call a value, e.g : tablea.tableb.func(Args)
	template<typename RT, typename ... Args>
	RT direct_call(lua_State *L, const char *func, const Args& ... args) {
		lua_pushcclosure(L, on_error, 0);
		int err_func = lua_gettop(L);

		lua_getglobal(L, "direct_get");
		if (lua_isfunction(L, -1)) {
			push(L, func);
			lua_pcall(L, 1, 1, err_func);
			if (lua_isfunction(L, -1)) {
				int arr[] = { 0, (push(L, args), 0)... };
				unused(arr); // avoid warnning
				lua_pcall(L, sizeof...(args), RetNum<RT>::value, err_func);
			}
			else {
				assert(false && "attemp to call a none function!");
			}
		}
		else {
			assert(false && "attemp to call a none function!");
		}

		lua_remove(L, err_func);

		return pop<RT>(L);
	}

	// register a c function into lua
	template<typename F>
	void def(lua_State *L, const char *name, F f) {
		lua_pushlightuserdata(L, (void *)f);
		push_method(L, f);
		lua_setglobal(L, name);
	}

	// direct def a table function, eg:tba.tbb.func = f
	template<typename F>
	void direct_def(lua_State *L, const char *tb, const char *name, F f) {
		lua_pushcclosure(L, on_error, 0);
		int err_func = lua_gettop(L);

		lua_getglobal(L, "direct_get");
		if (lua_isfunction(L, -1)) {
			push(L, tb);
			lua_pcall(L, 1, 1, err_func);
			if (lua_isnil(L, -1)) {
				lua_pop(L, 1);
				lua_newtable(L);

				lua_pushstring(L, "__name");
				lua_pushstring(L, tb);
				lua_rawset(L, -3);

				lua_setglobal(L, tb);
				lua_getglobal(L, tb);
			}

			if (lua_istable(L, -1)) {
				lua_pushstring(L, name);
				lua_pushlightuserdata(L, (void *)f);
				push_method(L, f);
				lua_rawset(L, -3);
			}
			else {
				assert(false && "not a table object.");
			}
		}
		else {
			assert(false && "not a function.");
		}

		lua_remove(L, err_func);
	}

	// register a var into lua
	template<typename T>
	void set(lua_State *L, const char *name, T t) {
		push(L, t);
		lua_setglobal(L, name);
	}

	// set a table var, eg:tba.tbb.va = v
	template<typename T>
	void direct_set(lua_State *L, const char *tb, const char *name, T t) {
		lua_pushcclosure(L, on_error, 0);
		int err_func = lua_gettop(L);

		lua_getglobal(L, "direct_get");
		if (lua_isfunction(L, -1)) {
			push(L, tb);
			lua_pcall(L, 1, 1, err_func);
			if (lua_isnil(L, -1)) {
				lua_pop(L, 1);
				lua_newtable(L);

				lua_pushstring(L, "__name");
				lua_pushstring(L, tb);
				lua_rawset(L, -3);

				lua_setglobal(L, tb);
				lua_getglobal(L, tb);
			}

			if (lua_istable(L, -1)) {
				lua_pushstring(L, name);
				push(L, t);
				lua_rawset(L, -3);
			}
			else {
				assert(false && "not a table object.");
			}
		}
		else {
			assert(false && "not a function.");
		}

		lua_remove(L, err_func);
	}

	// get a var from lua
	template<typename T>
	T get(lua_State *L, const char *name) {
		lua_getglobal(L, name);
		return pop<T>(L);
	}

	// get table var directly, such as, t.a.b
	template<typename T>
	T direct_get(lua_State *L, const char *name) {
		return call<T, const char *>(L, "direct_get", name);
	}

	// register a class into lua
	template<typename T>
	void object_reg(lua_State *L, const char *name) {
		ObjectName<typename base<T>::type>::reg_name(name);

		lua_getglobal(L, name);
		assert(lua_isnil(L, -1) && "object rereg!");
		lua_pop(L, 1);

		lua_newtable(L);

		lua_pushstring(L, "__name");
		lua_pushstring(L, name);
		lua_rawset(L, -3);

		lua_pushstring(L, "__gc");
		lua_pushcclosure(L, destructor<T>, 0);
		lua_rawset(L, -3);

		lua_pushstring(L, "__index");
		lua_pushvalue(L, -2);
		lua_rawset(L, -3);

		lua_setglobal(L, name);
	}

	// register class constructor into lua
	template<typename T, typename F>
	void object_constructor_reg(lua_State *L, F f) {
		lua_getglobal(L, ObjectName<typename base<T>::type>::name);
		if (lua_istable(L, -1)) {
			lua_pushstring(L, "create");
			lua_pushcclosure(L, f, 0);
			lua_rawset(L, -3);
		}
		else {
			assert(false && "not an object");
		}
		lua_pop(L, 1);
	}

	// register class method into lua
	template<typename T, typename F>
	void object_method_reg(lua_State *L, const char *name, F f) {
		lua_getglobal(L, ObjectName<typename base<T>::type>::name);
		if (lua_istable(L, -1)) {
			lua_pushstring(L, name);
			new(lua_newuserdata(L, sizeof(F))) F(f);
			//lua_pushlightuserdata(L, (void *)&f);
			push_method(L, f);
			lua_rawset(L, -3);
		}
		else {
			assert(false && "not an object");
		}
	}

	// def class inheritance
	template<typename C, typename P>
	int object_inh(lua_State *L) {
		lua_getglobal(L, ObjectName<typename base<C>::type>::name);
		assert(lua_istable(L, -1) && "not an object");
		lua_getglobal(L, ObjectName<typename base<P>::type>::name);
		assert(lua_istable(L, -1) && "not an object");
		lua_setmetatable(L, -2);
	}

} // end namespace luawrapper 

#endif // __LUA_WRAPPER_H_

