-- Example Lua UI plugin: shows a notification and demonstrates chat on login.

mko.log(1, "UI demo Lua plugin loaded")

mko.show_notification("Lua UI demo is now active.")

mko.register_on_message(function(msg_name, llsd_notation)
    if msg_name == "AgentStateUpdate" then
        mko.chat("Hello from Lua!", 1)  -- 1 = normal chat
        mko.show_notification("Lua: AgentStateUpdate received")
    end
    return false
end)
