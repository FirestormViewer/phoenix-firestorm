-- Example Lua protocol plugin: blocks (swallows) specific message names.

mko.log(1, "Filter Lua plugin loaded")

local blocked = {
    ScriptDialog = true,
    ScriptControl = true
}

mko.register_on_message(function(msg_name, llsd_notation)
    if blocked[msg_name] then
        mko.log(2, "[filter.lua] blocked: " .. msg_name)
        return true  -- stop default viewer handler
    end
    return false
end)
