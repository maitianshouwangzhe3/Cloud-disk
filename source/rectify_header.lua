
local url = ngx.ctx.client_request_uri
if url == "/api/myfiles?cmd=normal" then
    ngx.header["Content-Length"] = tostring(tonumber(ngx.header["Content-Length"]) + 39)
elseif url == "/api/sharepic?cmd=browse" then
    ngx.header["Content-Length"] = tostring(tonumber(ngx.header["Content-Length"]) + 17)
end