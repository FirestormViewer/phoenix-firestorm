-- Example Lua protocol plugin: logs a greeting and every message it sees.

mko.log(1, "Hello from the Lua plugin!")

mko.register_on_message(function(msg_name, llsd_notation)
    mko.log(1, "[hello.lua] saw message: " .. msg_name)
    return false  -- do not mark as handled
end)
