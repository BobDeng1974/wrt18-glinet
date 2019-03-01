-- Create By Wind.
-- workshuo@126.com  2015-12-14

function fork_exec(command)
	local pid = nixio.fork()
	if pid > 0 then
		return
	elseif pid == 0 then
		-- change to root dir
		nixio.chdir("/")

		-- patch stdin, out, err to /dev/null
		local null = nixio.open("/dev/null", "w+")
		if null then
			nixio.dup(null, nixio.stderr)
			nixio.dup(null, nixio.stdout)
			nixio.dup(null, nixio.stdin)
			if null:fileno() > 2 then
				null:close()
			end
		end

		-- replace with target command
		nixio.exec("/bin/sh", "-c", command)
	end
end

function action_flashops(downmd5)
	local sys = require "luci.sys"
	local fs  = require "nixio.fs"

	local upgrade_avail = fs.access("/lib/upgrade/platform.sh")
	local reset_avail   = os.execute([[grep '"rootfs_data"' /proc/mtd >/dev/null 2>&1]]) == 0

	local restore_cmd = "tar -xzC/ >/dev/null 2>&1"
	local backup_cmd  = "sysupgrade --create-backup - 2>/dev/null"
	local image_tmp   = "/tmp/firmware.img"

	local function image_supported()
		return (os.execute("sysupgrade -T %q >/dev/null" % image_tmp) == 0)
	end

	local function image_checksum()
		return (luci.sys.exec("md5sum %q" % image_tmp):match("^([^%s]+)"))
	end

	local function storage_size()
		local size = 0
		if fs.access("/proc/mtd") then
			for l in io.lines("/proc/mtd") do
				local d, s, e, n = l:match('^([^%s]+)%s+([^%s]+)%s+([^%s]+)%s+"([^%s]+)"')
				if n == "linux" or n == "firmware" then
					size = tonumber(s, 16)
					break
				end
			end
		elseif fs.access("/proc/partitions") then
			for l in io.lines("/proc/partitions") do
				local x, y, b, n = l:match('^%s*(%d+)%s+(%d+)%s+([^%s]+)%s+([^%s]+)')
				if b and n and not n:match('[0-9]') then
					size = tonumber(b) * 1024
					break
				end
			end
		end
		return size
	end

	local disk_size = storage_size()
	local image_size = (fs.stat(image_tmp, "size") or 0)
	
	if(upgrade_avail ~= true) then
		print "system upgrade is invalid"
		return "system upgrade is invalid"
	end
	
	if(reset_avail ~= true) then
		print "system reset is invalid"
		return "system reset is invalid"
	end
	
	if(image_supported() ~= true) then
		print "system reset is invalid"
		return "Unsupport image file"
	end
	
	if(image_size > disk_size) then
		print "system reset is invalid"
		return "Upgrade image size > system disk_size"
	end
	
	--print("in md5",downmd5)
	--print("calc md5",image_checksum())
	if(downmd5 ~= image_checksum()) then
		print "Upgrade image MD5 check Failed"
		return "MD5_ERR" .. image_checksum()
	end
	
	if(image_size ~=0) then
		local keep = ""  -- '-n' to clear all config
		fork_exec("killall dropbear uhttpd crond; sleep 1; /sbin/sysupgrade %s %q" %{ keep, image_tmp })
		return "OK,flashing..."
	else
		return "Upgrade image size = 0"
	end
end


