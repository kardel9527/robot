#include "luawrapper.h"

namespace luawrapper {

	lua_State* init() {
		lua_State *L = luaL_newstate();
		luaL_openlibs(L);

		static const char *direct_get = "\
	function direct_get(var) \
		if var == nil then return nil; end \
		local v = _G; \
		if v == nil then return nil; end \
		for w in string.gmatch(var, \"[%w_]+\") do\
			v = v[w];\
			if v == nil then return nil; end \
		end \
		return v;\
	end";

		dostring(L, direct_get);
		return L;
	}

	void uninit(lua_State *L) {
		lua_close(L);
	}

	void dofile(lua_State *L, const char *file) {
		assert(L && file && "empty pointer!");
		lua_pushcclosure(L, on_error, 0);
		int err_func = lua_gettop(L);
		if (luaL_loadfile(L, file) == 0) {
			lua_pcall(L, 0, 1, err_func);
		}
		else {
			printf("lua error:%s\n", lua_tostring(L, -1));
		}

		lua_remove(L, err_func);
		lua_pop(L, 1);
	}

	void dostring(lua_State *L, const char *str) {
		dobuffer(L, str, strlen(str));
	}

	void dobuffer(lua_State *L, const char *buff, unsigned sz) {
		assert(L && buff && "null pointer!");

		lua_pushcclosure(L, on_error, 0);
		int err_func = lua_gettop(L);
		if (luaL_loadbuffer(L, buff, sz, "dobuffer") == 0) {
			lua_pcall(L, 0, 1, err_func);
		}
		else {
			printf("lua error:%s\n", lua_tostring(L, -1));
		}

		lua_remove(L, err_func);
		lua_pop(L, 1);
	}

	void enum_stack(lua_State *L) {
		int top = lua_gettop(L);
		printf("Type:%d\n", top);
		for (int i = 1; i <= lua_gettop(L); ++i)
		{
			switch (lua_type(L, i))
			{
			case LUA_TNIL:
				printf("lua_stack[%d:%s]\n", i, lua_typename(L, lua_type(L, i)));
				break;
			case LUA_TBOOLEAN:
				printf("lua_stack[%d:%s %s]\n", i, lua_typename(L, lua_type(L, i)), lua_toboolean(L, i) ? "true" : "false");
				break;
			case LUA_TLIGHTUSERDATA:
				printf("lua_stack[%d:%s 0x%08p]\n", i, lua_typename(L, lua_type(L, i)), lua_topointer(L, i));
				break;
			case LUA_TNUMBER:
				printf("lua_stack[%d:%s %f]\n", i, lua_typename(L, lua_type(L, i)), lua_tonumber(L, i));
				break;
			case LUA_TSTRING:
				printf("lua_stack[%d:%s %s]\n", i, lua_typename(L, lua_type(L, i)), lua_tostring(L, i));
				break;
			case LUA_TTABLE:
				printf("lua_stack[%d:%s 0x%8p]\n", i, lua_typename(L, lua_type(L, i)), lua_topointer(L, i));
				break;
			case LUA_TFUNCTION:
				printf("lua_stack[%d:%s() 0x%08p]\n", i, lua_typename(L, lua_type(L, i)), lua_topointer(L, i));
				break;
			case LUA_TUSERDATA:
				printf("lua_stack[%d:%s 0x%08p]\n", i, lua_typename(L, lua_type(L, i)), lua_topointer(L, i));
				break;
			case LUA_TTHREAD:
				printf("lua_stack[%d:%s]\n", i, lua_typename(L, lua_type(L, i)));
				break;
			default:
				break;
			}
		}
	}

	static void call_stack(lua_State *L, int n) {
		lua_Debug d;
		if (lua_getstack(L, n, &d) == 1) {
			lua_getinfo(L, "nSlu", &d);

			if (n == 0) {
				printf("lua call stack begin:\n");
			}
			else {
				if (d.name)
					printf("%s() : line %d [%s : line %d]\n", d.name, d.currentline, d.source, d.linedefined);
				else
					printf("unknown : line %d [%s : line %d]\n", d.currentline, d.source, d.linedefined);
			}

			call_stack(L, n + 1);
		}
	}

	int on_error(lua_State *L) {
		printf("lua error:%s\n", lua_tostring(L, -1));
		call_stack(L, 0);

		return 0;
	}

	template<> bool read(lua_State *L, int idx) { return lua_isboolean(L, idx) ? lua_toboolean(L, idx) != 0 : lua_tointeger(L, idx) != 0; }
	template<> char read(lua_State *L, int idx) { return (char)lua_tointeger(L, idx); }
	template<> signed char read(lua_State *L, int idx) { return (signed char)lua_tointeger(L, idx); }
	template<> unsigned char read(lua_State *L, int idx) { return (unsigned char)lua_tointeger(L, idx); }
	template<> short read(lua_State *L, int idx) { return (short)lua_tointeger(L, idx); }
	template<> unsigned short read(lua_State *L, int idx) { return (unsigned short)lua_tointeger(L, idx); }
	template<> int read(lua_State *L, int idx) { return (int)lua_tointeger(L, idx); }
	template<> unsigned int read(lua_State *L, int idx) { return (unsigned int)lua_tointeger(L, idx); }
	template<> long read(lua_State *L, int idx) { return (long)lua_tointeger(L, idx); }
	template<> unsigned long read(lua_State *L, int idx) { return (unsigned long)lua_tointeger(L, idx); }
	template<> long long read(lua_State *L, int idx) { return (long long)lua_tointeger(L, idx); }
	template<> unsigned long long read(lua_State *L, int idx) { return (unsigned long long)lua_tointeger(L, idx); }
	template<> float read(lua_State *L, int idx) { return (float)lua_tonumber(L, idx); }
	template<> double read(lua_State *L, int idx) { return (double)lua_tonumber(L, idx); }
	template<> const char* read(lua_State *L, int idx) { return lua_tostring(L, idx); }
	template<> void read(lua_State *L, int idx) { return; }

	template<> void pop(lua_State *L) {}

	template<> void push(lua_State *L, bool val) { lua_pushboolean(L, val); }
	template<> void push(lua_State *L, char val) { lua_pushinteger(L, val); }
	template<> void push(lua_State *L, signed char val) { lua_pushinteger(L, val); }
	template<> void push(lua_State *L, unsigned char val) { lua_pushinteger(L, val); }
	template<> void push(lua_State *L, short val) { lua_pushinteger(L, val); }
	template<> void push(lua_State *L, unsigned short val) { lua_pushinteger(L, val); }
	template<> void push(lua_State *L, int val) { lua_pushinteger(L, val); }
	template<> void push(lua_State *L, unsigned int val) { lua_pushinteger(L, val); }
	template<> void push(lua_State *L, long val) { lua_pushinteger(L, val); }
	template<> void push(lua_State *L, unsigned long val) { lua_pushinteger(L, val); }
	template<> void push(lua_State *L, long long val) { lua_pushinteger(L, val); }
	template<> void push(lua_State *L, unsigned long long val) { lua_pushinteger(L, val); }
	template<> void push(lua_State *L, float val) { lua_pushnumber(L, val); }
	template<> void push(lua_State *L, double val) { lua_pushnumber(L, val); }
	template<> void push(lua_State *L, const char *val) { lua_pushstring(L, val); }
} // end namespace luawrapper
