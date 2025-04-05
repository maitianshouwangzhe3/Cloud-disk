
local url = ngx.ctx.client_request_uri
if url == "/api/myfiles?cmd=normal" then
    local bklist = ngx.shared.bklist
    local public_ip = bklist:get("cloud_disk_rectify_ip")
    if public_ip then
        local chunk = ngx.arg[1]
        if chunk then
            -- 将响应体拼接成完整的内容
            local ctx = ngx.ctx

            ctx.body_chunks = (ctx.body_chunks or "") .. chunk
            -- 完整的响应体
            local body = ctx.body_chunks
            -- 解析 JSON 响应
            local cjson = require "cjson"
            local json_data, err = cjson.decode(tostring(body))
            if not json_data then
                ngx.log(ngx.INFO, "Failed to decode JSON: ", err)
                return
            end

            -- 修改 JSON 中的 url 字段
            if type(json_data.files) == "table" then
                for _, file in ipairs(json_data.files) do
                    -- 修改每个对象的 url 字段
                    if file.url then
                        file.url = file.url:gsub("192.168.31.245", public_ip)
                    end
                end
            end

            -- 重新生成 JSON
            local modified_body = cjson.encode(json_data)
            -- 更新响应体
            ngx.arg[1] = modified_body
        end
    end
elseif url == "/api/sharepic?cmd=browse" then
    local bklist = ngx.shared.bklist
    local public_ip = bklist:get("cloud_disk_rectify_ip")
    if public_ip then
        if chunk then
            -- 将响应体拼接成完整的内容
            local ctx = ngx.ctx

            ctx.body_chunks = (ctx.body_chunks or "") .. chunk
            -- 完整的响应体
            local body = ctx.body_chunks
            -- 解析 JSON 响应
            local cjson = require "cjson"
            local json_data, err = cjson.decode(tostring(body))
            if not json_data then
                ngx.log(ngx.INFO, "Failed to decode JSON: ", err)
                return
            end

            if json_data.url then
                json_data.url = json_data.url:gsub("192.168.31.245", public_ip)
            end

            -- 重新生成 JSON
            local modified_body = cjson.encode(json_data)
            -- 更新响应体
            ngx.arg[1] = modified_body
        end
    end
end
