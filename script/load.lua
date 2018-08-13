package.path = package.path .. ";./../client/game/Assets/Res/TLua/CommonScript/protocol/?.txt";
local auth_key = "qwetrsdfas";

local robot_num = 300;
--local game_host = "192.168.37.192";
local game_host = "192.168.36.252";
local game_port = 5221;
local conn_num = 0;
local conn = {};
local last_calc_ping_time = 0;
local msg_cb = {};
local RobotStatus = {
	None = 0,
	Loginning = 1,
	Logined = 2,
	Matching = 3,
	Matched = 4,
	EnteringScene = 5,
	EnteredScene = 6,
	Fighting = 7
};

function registerMsgCallback(msgId, fun)
	msg_cb[msgId] = fun;
end

function unregisterMsgCallback(msgId)
	msg_cb[msgId] = nil;
end

local function initProto()
	local pbpath = "./../client/game/Assets/Res/TLua/CommonScript/protocol/";
	require("pb");
	local protobuf = require("./script/protobuf");
	local file = nil;
	for _, pb in ipairs(protocol_pb) do
		file = string.format("%s%s.bytes", pbpath, pb);
		protobuf.register_file(file, _G);
	end
	
	_G.msgencode = protobuf.encode;
	_G.msgdecode = protobuf.decode;
	_G.msgrootname = "PbProtocol.ProtocolBody"	-- 消息根包名
	_G.opcode = _G.PbProtocol.MsgIdDef	-- 消息id定义包
	_G.EServiceType = _G.PbProtocol.EServiceType
end

-- init the pbc.
initProto();

-- connect to the game host
function onStart()
	-- init random
	math.randomseed(os.time());

	-- connect all robot.
	local proactor = s_robot:proactor();
	for i = 1, robot_num, 1 do
		local handle = proactor:connect(game_host, game_port);
		if (handle) then
			conn[handle:id()] = { handle = handle, status = RobotStatus.None, sceneId = 0,index = 0, pos={},dir={} };
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
	local opcode = msg:opcode();
	--print("received opcode:"..opcode);
	local cb = msg_cb[opcode];
	if not cb then
		--print("opcode [" .. opcode .. "] callback not found!");
		return ;
	end
	
	local info, error_info = msgdecode(msgrootname, msg:data(), msg:len());
	if not info then
		print("msg decode failed:" .. error_info);
		return ;
	end
	
	cb(handle, info);
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

local function calcAvgSendBytes()
	local ret = 0.;
	for i, v in pairs(conn) do
		handle = v["handle"];
		ret = ret + handle:send_bytes();
	end

	if conn_num > 0 then
		return ret/conn_num;
	else
		return 0;
	end
end

local function calcAvgRecvBytes()
	local ret = 0.;
	for i, v in pairs(conn) do
		handle = v["handle"];
		ret = ret + handle:recv_bytes();
	end

	if conn_num > 0 then
		return ret/conn_num;
	else
		return 0;
	end
end

local function calcAvgSendSpeed()
	local ret = 0.;
	for i, v in pairs(conn) do
		handle = v["handle"];
		ret = ret + handle:avg_send_flow();
	end

	if conn_num > 0 then
		return ret/conn_num;
	else
		return 0;
	end
end

local function calcAvgRecvSpeed()
	local ret = 0.;
	for i, v in pairs(conn) do
		handle = v["handle"];
		ret = ret + handle:avg_recv_flow();
	end

	if conn_num > 0 then
		return ret/conn_num;
	else
		return 0;
	end
end

local function perfShow()
	print("avg ping: " .. calcAvgPing() .. "\n");
	print("avg send bytes:" .. calcAvgSendBytes() .. "\n");
	print("avg recv bytes:" .. calcAvgRecvBytes() .. "\n");
	print("avg send speed(bytes/s):" .. calcAvgSendSpeed() * 1000 .. "\n");
	print("avg recv speed(bytes/s):" .. calcAvgRecvSpeed() * 1000 .. "\n");
	print("now online robot[" .. conn_num .. "]\n");
	print("\n");
end

local robotId = 10000;
local function getRobotId()
	robotId = robotId + 1;
	return robotId;
end

local CSLoginReq_Body = { CSLoginReq = { accountId = 0, session = "", loginType = 1, timeStamp = 0, clientInfo = { deviceId = "", deviceType="PC", deviceMac="aaa", inCountry="", phoneType = "pc", deviceName="pc", deviceVersion="1", osVersion="v1.1", osName="win", isBreak = false, clientVersion="1.1", network="WIFI", resolution="1024*768", region_id="11", language="cn"}, uin=""} };

local function onRobotLoginBack(handle, body)
	local robot = conn[handle:id()];
	if not robot then print("robot object not found!"); return ; end
	print("robot idx[" .. handle:id() .. "]login success [globalid:" .. body.SCLoginRes.globalId .. "]!");
	robot.status = RobotStatus.Logined;
end

registerMsgCallback(opcode.MsgId_S2C_Login_Res, onRobotLoginBack);

local function doRobotLogin(robot)
	--print("begin robot login.");
	local accountId = robot.handle:id();
	CSLoginReq_Body.CSLoginReq.accountId = accountId;
	CSLoginReq_Body.CSLoginReq.uin = "robot"..accountId;
	
	local check_str = string.format("%s%s%s", tostring(accountId), tostring(CSLoginReq_Body.CSLoginReq.timeStamp), auth_key);
	CSLoginReq_Body.CSLoginReq.session = md5(check_str);

	local data = msgencode(msgrootname, CSLoginReq_Body);
	robot.handle:send(opcode.MsgId_C2S_Login_Req, data, #data);

	robot.status = RobotStatus.Loginning;
end

local function onMatchResult(handle, body)
	--print("robot match sceneId:"..body.SCSceneInfoNotify.sceneId);
	local robot = conn[handle:id()];
	if not robot then print("robot [" .. hanlde:id() .. "] not found."); return ; end

	robot.sceneId = body.SCSceneInfoNotify.sceneId;
	robot.status = RobotStatus.Matched;
end

registerMsgCallback(opcode.MsgId_S2C_Scene_Info_Notify, onMatchResult);

local function doRobotMatch(robot)
	--print("begin robot match");
	CSBeginMatchReq_Body = { CSBeginMatchReq = { sceneTblId = math.random(1, 7) } };
	local data = msgencode(msgrootname, CSBeginMatchReq_Body);
	robot.handle:send(opcode.MsgId_C2S_Begin_Match_Req, data, #data);
	robot.status = RobotStatus.Matching;
end

local function onSceneActorInfo(handle, body)
	local robot = conn[handle:id()];
	if not robot then print("robot [" .. hanlde:id() .. "] not found."); return ; end

	for i, v in pairs(body.SCActorInfoNotify.SCActorInfoList) do
		if v.id == handle:id() then
			print("received robot[" .. handle:id() .. "] actor info[idx:" .. v.index .. "]");
			robot.index = v.index;
			robot.pos = robot.pos or {};
			robot.pos.x = v.posData.pos.x;
			robot.pos.y = v.posData.pos.y;
			robot.pos.z = v.posData.pos.z;
			robot.dir = robot.dir or {};
			robot.dir.x = v.posData.dir.x;
			robot.dir.y = v.posData.dir.y;
			robot.dir.z = v.posData.dir.z;
			--robot.dir.w = v.posData.dir.w;
			robot.status = RobotStatus.Fighting;
		end
	end
end

registerMsgCallback(opcode.MsgId_S2C_Actor_Info_Notify, onSceneActorInfo);

local function doRobotEnterScene(robot)
	print("begin robot enter scene[" .. robot.sceneId .. "].");
	local CSEnterSceneReq_Body = { CSEnterSceneReq = { sceneId = robot.sceneId } };
	local data = msgencode(msgrootname, CSEnterSceneReq_Body);
	robot.handle:send(opcode.MsgId_C2S_Enter_Scene_Req, data, #data);
	robot.status = RobotStatus.EnteringScene;
end

local function doRobotAction(robot)
	--print("begin robot action.");
	robot.last_move_time = robot.last_move_time or ms_proccess();
	-- move each 60 ms
	if (ms_proccess() - robot.last_move_time) < 60 then return ; end;

	local pos = robot.pos;
	pos.x = pos.x + 100;
	pos.y = pos.y;
	pos.z = pos.z + 100;
	
	--print("move x:"..pos.x..", y:"..pos.y..", z:"..pos.z);

	local dir = robot.dir;
	dir.x = 100;
	dir.y = 173;
	dir.z = 100;
	dir.w = 30;
	
	local velocity = {x=100, y=100,z=100};

	-- do move
	local CSMoveReq_Body = { PhysicSyncState = { index=robot.index,pos={x=pos.x, y=pos.y, z=pos.z},dir={x=dir.x, y=dir.y, z=dir.z, w=dir.w}, velocity={x=velocity.x, y=velocity.y, z=velocity.z}, timeStamp=ms_proccess() }};

	local data = msgencode(msgrootname, CSMoveReq_Body);
	robot.handle:send(opcode.MsgId_C2S_PhysicMove_Req, data, #data);
end

local function onFightOver(handle, body)
	local robot = conn[handle:id()];
	if not robot then print("robot [" .. hanlde:id() .. "] not found."); return ; end

	-- when fight over set status to logined, and rematch.
	robot.status = RobotStatus.Logined;
end

registerMsgCallback(opcode.MsgId_S2C_Fight_Over_Notify, onFightOver);

local function updateRobot()
	for i, v in pairs(conn) do
		if v.status == RobotStatus.None then
			doRobotLogin(v);
		elseif v.status == RobotStatus.Logined then
			doRobotMatch(v);
		elseif v.status == RobotStatus.Matched then
			doRobotEnterScene(v);
		elseif v.status == RobotStatus.Fighting then
			doRobotAction(v);
		end
	end
end

function update()
	updateRobot();
	if ms_proccess() - last_calc_ping_time >= 2000 then
		last_calc_ping_time = ms_proccess();
		perfShow();
	end
	--print(s_robot:proactor())
end