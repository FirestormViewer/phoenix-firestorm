-- Auto-initialize the mko_discord plugin and keep rich presence in sync.

local APP_ID = "1263849976069623861"

local function get_setting(name, default)
    local v = mko.get_setting(name)
    if v == nil then return default end
    return v
end

local function register_settings()
    mko.register_setting("discord_activity_name", "Manikineko Online",
                         "Activity name/title", "string")
    mko.register_setting("discord_details_format", "{display_name} ({user_name})",
                         "Details format string", "string")
    mko.register_setting("discord_state_format", "{region} on {grid}",
                         "State format string", "string")
    mko.register_setting("discord_large_image", "",
                         "Large image asset key", "string")
    mko.register_setting("discord_large_text", "Manikineko Online",
                         "Large image hover text", "string")
    mko.register_setting("discord_small_image", "",
                         "Small image asset key", "string")
    mko.register_setting("discord_small_text", "",
                         "Small image hover text", "string")
    mko.register_setting("discord_button1_label", "",
                         "Button 1 label", "string")
    mko.register_setting("discord_button1_url", "",
                         "Button 1 URL", "string")
    mko.register_setting("discord_button2_label", "",
                         "Button 2 label", "string")
    mko.register_setting("discord_button2_url", "",
                         "Button 2 URL", "string")
    mko.register_setting("discord_show_start_time", "true",
                         "Show elapsed session time", "boolean")
end

local function send_init()
    mko.send_message("MkoDiscord", "{'command':'init','args':{'app_id':'" .. APP_ID .. "'}}")
end

local function send_connect()
    mko.send_message("MkoDiscord", "{'command':'connect'}")
end

local g_info = {}
local g_ready = false
local g_start_time = os.time()

local function parse_info(notation)
    local text = string.match(notation, "'text':'([^']*)")
    if not text then return end
    local info = {}
    for pair in string.gmatch(text, "([^|]+)") do
        local k, v = string.match(pair, "^([^=]+)=(.*)$")
        if k then
            info[k] = v
        end
    end
    return info
end

local function apply_format(fmt, info)
    if not fmt then return "" end
    local s = fmt
    for k, v in pairs(info) do
        s = string.gsub(s, "{" .. k .. "}", v)
    end
    -- remove any remaining unknown placeholders
    s = string.gsub(s, "{[^}]+}", "")
    return s
end

local function llsd_escape(s)
    s = tostring(s)
    s = string.gsub(s, "\\", "\\\\")
    s = string.gsub(s, "'", "\\'")
    s = string.gsub(s, "\n", "\\n")
    return "'" .. s .. "'"
end

local function append_field(parts, key, value)
    if value == nil or value == "" then return end
    if #parts > 0 then table.insert(parts, ",") end
    table.insert(parts, "'" .. key .. "':" .. llsd_escape(value))
end

local function append_int_field(parts, key, value)
    if value == nil then return end
    if #parts > 0 then table.insert(parts, ",") end
    table.insert(parts, "'" .. key .. "':i" .. tostring(value))
end

local function send_activity()
    if not g_ready then return end

    local parts = {}

    local name = get_setting("discord_activity_name", "Manikineko Online")
    append_field(parts, "name", name)

    local details = apply_format(get_setting("discord_details_format", ""), g_info)
    append_field(parts, "details", details)

    local state = apply_format(get_setting("discord_state_format", ""), g_info)
    append_field(parts, "state", state)

    if get_setting("discord_show_start_time", "true") == "true" then
        append_int_field(parts, "start_timestamp", g_start_time)
    end

    local large_image = get_setting("discord_large_image", "")
    local large_text = get_setting("discord_large_text", "")
    local small_image = get_setting("discord_small_image", "")
    local small_text = get_setting("discord_small_text", "")
    if large_image ~= "" or large_text ~= "" or small_image ~= "" or small_text ~= "" then
        append_field(parts, "large_image", large_image)
        append_field(parts, "large_text", large_text)
        append_field(parts, "small_image", small_image)
        append_field(parts, "small_text", small_text)
    end

    for i = 1, 2 do
        local label = get_setting("discord_button" .. i .. "_label", "")
        local url = get_setting("discord_button" .. i .. "_url", "")
        if label ~= "" and url ~= "" then
            append_field(parts, "button" .. i .. "_label", label)
            append_field(parts, "button" .. i .. "_url", url)
        end
    end

    local body = "{'command':'set_activity','args':{" .. table.concat(parts) .. "}}"
    mko.send_message("MkoDiscord", body)
end

mko.log(1, "discord_activity: initializing Discord Social SDK")
register_settings()

mko.register_on_message(function(msg_name, llsd_notation)
    if msg_name == "MkoViewerInfo" then
        local info = parse_info(llsd_notation)
        if info then
            g_info = info
            if info.logged_in == "true" then
                send_activity()
            end
        end
        return false
    end

    if msg_name == "MkoDiscordStatus" then
        mko.log(1, "discord_activity: status update: " .. llsd_notation)
        if string.find(llsd_notation, "'status':'Ready'") then
            g_ready = true
            mko.log(1, "discord_activity: Discord ready, setting rich presence")
            send_activity()
        end
        return false
    end

    return false
end)

send_init()
send_connect()
