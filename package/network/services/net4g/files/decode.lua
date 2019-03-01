#!/bin/env lua

local json = require "cjson.safe"

timeconfig = {file="/etc/timing_config"}
function timeconfig.read()
	local file = timeconfig.file
    local f = io.open(file, "r")
	if(f == nil) then
		print("timeconfig_read cannot open timeconfig file:",file)
		return ""
	end
    local s = f:read("*a")
	s = string.gsub(s,"\r","")
	s = string.gsub(s,"\n","")
    f:close()
    return s
end

function timeconfig.write(content)
	local file = timeconfig.file
    local f = io.open(file, "w+")
	if(f == nil) then
		print("timeconfig_write cannot open timeconfig file:",file)
		return false
	end
    f:write(content)
    f:close()
	return true
end

function timeconfig.remove()
    os.remove(timeconfig.file)
end

local function gpio_read(file)
	if(file == nil) then
		print(file," file path error!");
		return false,""
	end
    local f = io.open(file, "r")
	if(f == nil) then
		print("Cannot Read gpio file:",file)
		return false,""
	end
	local s = f:read("*a")
	s = string.gsub(s,"\r","")
	s = string.gsub(s,"\n","")
    f:close()
	return true,s
end

local function gpio_write(file,content)
	if(file == nil) then
		print(file," file is not exist!");
		return false
	end
    local f = io.open(file, "r+")
	if(f == nil) then
		print("Cannot Write gpio file:",file)
		return false
	end
	if(content) then
		f:write(content)
	end
    f:close()
	return true
end

local function delay_time(time)
	local delay_time = time * 1000000
	for i=1,delay_time,1 do
		
	end
end

local function gpio_read_dir(gpio_dir)
	return gpio_read(gpio_dir .. "/" .. "direction")
end

local function gpio_read_value(gpio_dir)
	return gpio_read(gpio_dir .. "/" .. "value")
end

local function gpio_export(number)
	local int_num = tonumber(number)
	if(int_num and int_num >=0 and int_num <= 99 ) then
		local sys_gpio_path = "/sys/class/gpio/gpio" .. number
		local ret,value = gpio_read_value(sys_gpio_path)
		if(ret == true) then
			print("GPIO",number,"Has been export,value=",value)
			return true
		end	
		
		print("To export new GPIO",number)
		
		-- export can NOT be opened
-- 		ret = gpio_write("/sys/class/gpio/export",number)
-- 		if(ret == false) then
-- 			return false
-- 		end
		local cmd = "/bin/echo " .. number .. " > /sys/class/gpio/export"
		os.execute(cmd)
		delay_time(5) -- RT5350  700ms
		ret,_ = gpio_read_dir(sys_gpio_path)
		return ret
	else
		print("ERROR! Invalid Gpio Number",number)
		return false
	end
end

local function gpio_write_dir(gpio_dir,content)
	return gpio_write(gpio_dir .. "/" .. "direction",content)
end

local function gpio_write_value(gpio_dir,content)
	return gpio_write(gpio_dir .. "/" .. "value",content)
end

local function gpio_check_params(ctrl_msg)
	if(ctrl_msg == nil) then
		print("ctrlmsg is nil")
		return false
	end
	
	local number = ctrl_msg["gpionum"]
	local direction = ctrl_msg["gpiodir"]
	local value = ctrl_msg["gpioval"]
	local name = ctrl_msg["gpioname"]
	
	if(number and direction and value and name) then
		int_num = tonumber(number)
		if(int_num and int_num >=0 and int_num <= 99 ) then
		else
			print("Number is Invalid!",int_num)
			return false
		end
		if(direction == "in" or direction == "out") then
		else
			print("Direction is Invalid!",direction)
			return false
		end
		if(value == "1" or value == "0") then
		else
			print("Value is Invalid!",value)
			return false
		end
	else
		print("gpio params is invalid")
		return false
	end
	return true
end

local function gpio_ctrl(ctrl_msg)
	if(gpio_check_params(ctrl_msg) == false) then
		print("[gpio_ctrl]Invalid GPIO ctrl paramters",ctrl_msg)
		return false,"Invalid Params,error"
	end
	
	if(ctrl_msg["gpioname"] ~= "") then
		local gpio_dev = "/sys/class/leds/" .. ctrl_msg["gpioname"] .. "/brightness"
		local ret,data = gpio_read(gpio_dev)
		if(ret == false) then
			print("Ctrl GPIO Read Failed!",gpio_dev)
			return false,"Read error"
		end
		if(data == ctrl_msg["gpioval"]) then
			print("no need to change")
			return true,"OK,val=" .. data
		end
		local cmd = "/bin/echo " .. ctrl_msg["gpioval"] .. " > " .. gpio_dev
		os.execute(cmd)
		ret,data = gpio_read(gpio_dev)
		if(ret == false) then
			print("Ctrl GPIO Read Failed!",gpio_dev)
			return false,"Read error"
		end
		if(data == ctrl_msg["gpioval"]) then
			print("Ctrl GPIO OK")
			return true,"OK,val=" .. data
		else
			print("Ctrl GPIO Failed")
			return false,"Update error"
		end
	else
		-- here No support
		print("now not support")
		return false,"name error"
	end
	
	-- if gpioname is empty,to use export gpio
	if(gpio_export(ctrl_msg["gpionum"]) == false) then
		print("Export GPIO Error",ctrl_msg["gpionum"])
		return false,"Export error"
	end
	
	local sys_gpio_path = "/sys/class/gpio/gpio"
	local gpio_num_path = sys_gpio_path .. ctrl_msg["gpionum"]
	local ret,data = gpio_read_dir(gpio_num_path)
	if(ret) then
		if(data ~= ctrl_msg["gpiodir"]) then
			gpio_write_dir(gpio_num_path,ctrl_msg["gpiodir"])
		end
	else
		-- direction file is not exist!
-- 		gpio_export(ctrl_msg["gpionum"])
		return false,"Direction error"
	end
	
	ret,data = gpio_read_value(gpio_num_path)
	if(ret) then
		if(data ~= ctrl_msg["gpioval"]) then
			gpio_write_value(gpio_num_path,ctrl_msg["gpioval"])
		end
	else
		-- direction file is not exist!
-- 		gpio_export(ctrl_msg["gpionum"])
		print("read val error")
		return false,"Value error"
	end
	return true,"OK,val=" .. data
end

--[[ detect timing is timeout or not ]]
function detect_timing_flag(ctrlmsg)
	--print("LUA_TIME,Begin Parse Ctrl Msg",os.clock(),os.date("%X"))
	local curtime = os.date("%H%M")
	local luaval = json.decode(ctrlmsg)
	if(luaval == nil) then
		print("decode_ctrl_msg Format error!",ctrlmsg)
		return "Format error"
	else
		if(luaval["body"] == nil) then
			print("decode_ctrl_msg Body error!",ctrlmsg)
			return "Body error"
		end
		--print("Server ctrl msg:",luaval["type"],luaval["seqnum"],luaval["body"])
		local timerange = luaval["body"]["timerange"]
		local workmode = "auto"
		if(timerange and type(timerange) == "table") then
			if(workmode == "auto") then
				for i,k in pairs(timerange) do
					--print("num,start,end",k["num"],k["st"],k["ed"])
					if(k["num"] == nil or k["st"] == nil or k["ed"] == nil) then
						print "time-key error"
						return "time-key error"
					end
					if(k["enb"] == "1" and k["st"] and k["ed"] and curtime >= k["st"] and curtime <= k["ed"]) then
						-- the all conditions are TRUE!
						print("DO GPIO",curtime,k["st"],k["ed"])
						--gpio ctrl
						local ret,msg = gpio_ctrl(luaval["body"]["gpioctrl"])
						return msg .. ":AUTO"
					end
				end
				return "OK:nothing"
			elseif(workmode == "manual") then
				local ret,msg = gpio_ctrl(luaval["body"]["gpioctrl"])
				return msg .. ":MANUAL"
			else
				print "ctrlmode error"
				return "ctrlmode error"
			end
		end
		return "Has no timerange,error"
	end
end

function gpio_reverse()
	local ctrlmsg = timeconfig.read()
	local luaval = json.decode(ctrlmsg)
	if(luaval == nil) then
		print("gpio_reverse Format error!",ctrlmsg)
		return "Format error"
	else
		if(luaval["body"] == nil) then
			print("gpio_reverse Body error!",ctrlmsg)
			return "Body error"
		end
		local gpio = luaval["body"]["gpioctrl"]
		if(gpio_check_params(gpio) == false) then
			print("[gpio_reverse]Invalid GPIO ctrl paramters",gpio)
			return "Invalid Params,error"
		end
		luaval["body"]["gpiomode"] = "manual"
		
		if(gpio["gpioval"] == "1") then
			gpio["gpioval"] = "0"
		elseif(gpio["gpioval"] == "0") then
			gpio["gpioval"] = "1"
		else
			print("Ctrl msg GPIO value is Invalid")
			return "Invalid GPIO value,error"
		end
		local ret,msg = gpio_ctrl(gpio)
		return msg
	end
end

function is_gpiotiming_valid(timing_info)
	local luaval = json.decode(timing_info)
	if(luaval == nil) then
		print("is_gpiotiming_valid Format error!",timing_info)
		return "Format error"
	else
		if(luaval["body"] == nil) then
			print("is_gpiotiming_valid Body error!",timing_info)
			return "Body error"
		end
		
		local gpio = luaval["body"]["gpioctrl"]
		if(gpio_check_params(gpio) == false) then
			print("[is_gpiotiming_valid]Invalid GPIO ctrl paramters",gpio)
			return "GPIO Params,error"
		else
			local timerange = luaval["body"]["timerange"]
			if(timerange and type(timerange) == "table") then
				for i,k in pairs(timerange) do
					if(k["num"] == nil or k["st"] == nil or k["ed"] == nil) then
					    print "time key error"
						return "time-key error"
					end
				end
				return "OK,valid"
			else
			    print "timerange error"
				return "timerange error"
			end
		end
	end
end

function is_ctrlmsg_valid(ctrlmsg)
	local luaval = json.decode(ctrlmsg)
	if(luaval == nil) then
		print("is_msg_valid Format error!",ctrlmsg)
		return "Format error"
	else
		if(luaval["body"] == nil) then
			print("is_msg_valid Body error!",ctrlmsg)
			return "Body error"
		end
		local mode = luaval["body"]["gpiomode"]
		local gpio = luaval["body"]["gpioctrl"]
		if(mode == nil) then
			print("not found gpiomode key")
			return "mode error"
		elseif(mode == "auto") then
			if(gpio_check_params(gpio) == false) then
				print("[is_msg_valid]Invalid GPIO ctrl paramters",gpio)
				return "GPIO Params,error"
			else
				local ret,msg = gpio_ctrl(gpio)
				return "auto," .. msg
			end
		elseif(mode == "manual") then
			if(gpio_check_params(gpio) == false) then
				print("[is_msg_valid]Invalid GPIO ctrl paramters",gpio)
				return "GPIO Params,error"
			else
				local ret,msg = gpio_ctrl(gpio)
				return "manual," .. msg
			end
		else
		    print "mode error"
			return "mode error"
		end
		
	end
end
-- local leds = "/sys/class/leds/hame:red:simcard/brightness"
-- print("gpio read leds",gpio_read(leds))
-- print (gpio_reverse())
-- print (detect_timing_flag(timeconfig.read()))








