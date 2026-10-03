-- Example Lua protocol plugin: auto-replies to a "TestPing" message.

mko.log(1, "Responder Lua plugin loaded")

mko.register_on_message(function(msg_name, llsd_notation)
    if msg_name == "TestPing" then
        mko.log(1, "[responder.lua] got TestPing, sending TestPong")
        mko.send_message("TestPong", "'pong'")
        return true  -- message handled
    end
    return false
end)
