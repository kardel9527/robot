
local robot_num = 100;
local game_host = "192.168.36.252";
local game_port = 5221;
local conn_num = 0;
local conn = {};
local last_calc_ping_time = 0;

-- connect to the game host
function onStart()
	local proactor = s_robot:proactor();
	for i = 1, robot_num, 1 do
		local handle = proactor:connect(game_host, game_port);
		if (handle) then
			conn[handle:id()] = { handle = handle };
			conn_num = conn_num + 1;
		end
	end

	print("totally connected ["..conn_num.."] robots.\n");
end

function onDisconnected(id)
	print("session [" .. id .. "] disconnected!");
	if id then
		if (conn[id]) then conn_num = conn_num - 1; end
		conn[id] = nil;
	end
end

function onNetPacket(handle, msg)
end

local function calcAvgPing()
	local total_avg_ping = 0.;
	for i, v in pairs(conn) do
		handle = v["handle"];
		total_avg_ping = total_avg_ping + handle:avg_ping();
	end

	if conn_num > 0 then
		return total_avg_ping/conn_num;
	else
		return 0;
	end
end

local function

local function perfShow()
	avg_ping
end

function update()
	if ms_proccess() - last_calc_ping_time >= 35000 then
		last_calc_ping_time = ms_proccess();
		print("ping:"..calcAvgPing().."\n");
	end
	--print(s_robot:proactor())
end