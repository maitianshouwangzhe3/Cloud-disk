if ngx.worker.id() ~= 0 then
    return
end

local redis = require "resty.redis"
local bklist = ngx.shared.bklist

local function update_blacklist()
    local red = redis:new()
    local ok, err = red:connect("127.0.0.1", 6379)
    if not ok then
        return
    end
    local black_list,err = red:get("cloud_disk_rectify_ip")
    bklist:flush_all()
    if not err then
        bklist:set("cloud_disk_rectify_ip", black_list)
    end
    red:close()
    ngx.timer.at(360, update_blacklist)
end

ngx.timer.at(5, update_blacklist)