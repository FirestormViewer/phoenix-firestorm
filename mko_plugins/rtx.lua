-- RTX/Raytracing plugin for Manikineko Online.
--
-- Loads shader packs from <plugin_dir>/rtx_shader_packs/<pack_name>/
-- and registers GLSL overrides with the viewer when rtx_enabled is true.
--
-- Chat commands (type in nearby chat):
--   /rtx on              enable RTX and reload shaders
--   /rtx off             disable RTX and reload shaders
--   /rtx pack <name>     switch to the named shader pack
--   /rtx reload          reload the current pack and shaders
--   /rtx list            list available shader packs
--   /rtx status          show current settings

local function get_setting(name, default)
    local v = mko.get_setting(name)
    if v == nil then return default end
    return v
end

local function set_setting(name, value)
    return mko.set_setting(name, tostring(value))
end

local DIR_SEP = package.config:sub(1, 1)

local PLUGIN_DIR = mko.get_plugin_dir() or "."
if PLUGIN_DIR:sub(-1) ~= "/" and PLUGIN_DIR:sub(-1) ~= "\\" then
    PLUGIN_DIR = PLUGIN_DIR .. DIR_SEP
end
local PACKS_DIR = PLUGIN_DIR .. "rtx_shader_packs"

local function log(level, msg)
    if mko and mko.log then
        mko.log(level, "[rtx] " .. msg)
    end
end

-- Quote a path for use with the platform shell.
local function shell_quote_path(s)
    if DIR_SEP == "\\" then
        -- Windows cmd.exe: use double quotes, double embedded quotes.
        return '"' .. string.gsub(s, '"', '""') .. '"'
    else
        -- POSIX shell: use single quotes, escape embedded single quotes.
        return "'" .. string.gsub(s, "'", "'\\''") .. "'"
    end
end

-- Join path components using the platform separator.
local function path_join(...)
    local parts = {...}
    local s = parts[1]
    for i = 2, #parts do
        if s:sub(-1) ~= "/" and s:sub(-1) ~= "\\" then
            s = s .. DIR_SEP
        end
        s = s .. parts[i]
    end
    return s
end

-- List subdirectories in PACKS_DIR using the platform shell.
-- Lua has no portable directory walker, so this is best-effort.
local function list_packs()
    local packs = {}
    if not PACKS_DIR then return packs end

    local cmd
    if DIR_SEP == "\\" then
        cmd = "dir /b " .. shell_quote_path(PACKS_DIR)
    else
        cmd = "ls -1 " .. shell_quote_path(PACKS_DIR)
    end

    local ok, handle = pcall(io.popen, cmd)
    if ok and handle then
        for line in handle:lines() do
            local name = string.match(line, "^([^/\\]+)$")
            if name then
                local manifest_path = path_join(PACKS_DIR, name, "manifest.lua")
                local mf = io.open(manifest_path, "r")
                if mf then
                    mf:close()
                    table.insert(packs, name)
                end
            end
        end
        handle:close()
    end
    return packs
end

local function read_file(path)
    local f, err = io.open(path, "r")
    if not f then
        return nil, err
    end
    local content = f:read("*all")
    f:close()
    return content
end

local function load_manifest(pack_name)
    local manifest_path = path_join(PACKS_DIR, pack_name, "manifest.lua")
    local chunk, err = loadfile(manifest_path)
    if not chunk then
        return nil, err
    end
    local ok, manifest = pcall(chunk)
    if not ok then
        return nil, manifest
    end
    if type(manifest) ~= "table" then
        return nil, "manifest did not return a table"
    end
    return manifest
end

local function load_pack(pack_name)
    local manifest, err = load_manifest(pack_name)
    if not manifest then
        log(3, "Failed to load pack '" .. pack_name .. "': " .. tostring(err))
        return false
    end

    if type(manifest.shaders) ~= "table" then
        log(3, "Pack '" .. pack_name .. "' has no shaders list")
        return false
    end

    local pack_base = path_join(PACKS_DIR, pack_name)
    local registered = 0
    for _, entry in ipairs(manifest.shaders) do
        local file_path = path_join(pack_base, entry.file)
        local source, read_err = read_file(file_path)
        if not source then
            log(3, "Could not read shader file " .. file_path .. ": " .. tostring(read_err))
        else
            local ok = mko.register_shader(entry.name, entry.type, source, entry.defines)
            if ok then
                registered = registered + 1
            else
                log(3, "Failed to register shader " .. entry.name)
            end
        end
    end

    log(1, "Registered " .. registered .. " shader(s) from pack '" .. pack_name .. "'")
    return registered > 0
end

local function apply_rtx()
    local enabled = get_setting("rtx_enabled", "false") == "true"
    if not enabled then
        return
    end
    local pack = get_setting("rtx_shader_pack", "default")
    log(1, "Loading RTX shader pack: " .. pack)
    load_pack(pack)
end

local function reload_shaders()
    mko.send_message("MkoReloadShaders", "{}")
end

local function clear_shader_overrides()
    mko.send_message("MkoClearShaderOverrides", "{}")
end

local function show_status()
    local enabled = get_setting("rtx_enabled", "false")
    local pack = get_setting("rtx_shader_pack", "default")
    mko.show_notification("RTX enabled: " .. enabled .. " | pack: " .. pack)
end

local function handle_command(cmd, pack_arg)
    if cmd == "on" then
        set_setting("rtx_enabled", "true")
        apply_rtx()
        reload_shaders()
        mko.show_notification("RTX enabled. Reloading shaders with pack '" .. get_setting("rtx_shader_pack", "default") .. "'.")
    elseif cmd == "off" then
        set_setting("rtx_enabled", "false")
        clear_shader_overrides()
        mko.show_notification("RTX disabled.")
    elseif cmd == "pack" then
        if pack_arg and pack_arg ~= "" then
            set_setting("rtx_shader_pack", pack_arg)
            if get_setting("rtx_enabled", "false") == "true" then
                clear_shader_overrides()
                apply_rtx()
                reload_shaders()
                mko.show_notification("Switched to RTX pack '" .. pack_arg .. "' and reloaded shaders.")
            else
                mko.show_notification("RTX pack set to '" .. pack_arg .. "'. Enable RTX with /rtx on.")
            end
        else
            mko.show_notification("Usage: /rtx pack <name>")
        end
    elseif cmd == "reload" then
        if get_setting("rtx_enabled", "false") == "true" then
            clear_shader_overrides()
            apply_rtx()
            reload_shaders()
            mko.show_notification("RTX shader pack reloaded.")
        else
            mko.show_notification("RTX is disabled. Use /rtx on first.")
        end
    elseif cmd == "list" then
        local packs = list_packs()
        if #packs == 0 then
            mko.show_notification("No RTX shader packs found in " .. PACKS_DIR)
        else
            mko.show_notification("RTX packs: " .. table.concat(packs, ", "))
        end
    elseif cmd == "status" then
        show_status()
    else
        mko.show_notification("Unknown RTX command. Try: /rtx on | off | pack <name> | reload | list | status")
    end
end

local function register_settings()
    mko.register_settings_tab("rtx", "RTX")
    mko.register_setting("rtx_enabled", "false", "Enable RTX/Raytracing", "boolean", "rtx")
    mko.register_setting("rtx_shader_pack", "default", "RTX shader pack name", "string", "rtx")
    mko.register_setting("rtx_mode", "basic", "RTX mode", "enum", "rtx", "basic|full")
end

-- Main initialization.
log(1, "RTX plugin loading")
register_settings()
apply_rtx()

mko.register_on_message(function(msg_name, llsd_notation)
    if msg_name == "MkoRTXCommand" then
        local cmd = string.match(llsd_notation, "'command':'([^']*)'") or ""
        local pack = string.match(llsd_notation, "'pack':'([^']*)'") or ""
        handle_command(cmd, pack)
        return true
    end

    if msg_name == "MkoServerShaderPolicy" then
        -- Supported grids may force a shader pack: { 'pack':'name', 'forced':b1 }
        local pack = string.match(llsd_notation, "'pack':'([^']*)'") or ""
        local forced = string.find(llsd_notation, "'forced':b1") ~= nil
        if pack ~= "" and forced then
            log(1, "Server forces shader pack '" .. pack .. "'")
            set_setting("rtx_shader_pack", pack)
            set_setting("rtx_enabled", "true")
            clear_shader_overrides()
            apply_rtx()
            reload_shaders()
            mko.show_notification("This grid requires shader pack '" .. pack .. "'. It has been applied.")
        elseif pack ~= "" then
            log(1, "Server suggests shader pack '" .. pack .. "'")
            mko.show_notification("This grid recommends shader pack '" .. pack .. "'. Apply with /rtx pack " .. pack)
        end
        return true
    end

    if msg_name == "MkoSettingChanged" then
        -- React to settings changed from the Preferences > Graphics plugin tab.
        local name = string.match(llsd_notation, "'name':'([^']*)'") or ""
        if name == "rtx_enabled" or name == "rtx_shader_pack" or name == "rtx_mode" then
            if get_setting("rtx_enabled", "false") == "true" then
                clear_shader_overrides()
                apply_rtx()
                reload_shaders()
            else
                clear_shader_overrides()
            end
        end
        return false
    end

    if msg_name == "MkoViewerInfo" then
        -- Notify the user on login that RTX can be toggled via chat.
        if string.find(llsd_notation, "logged_in='true'") then
            local enabled = get_setting("rtx_enabled", "false") == "true"
            local pack = get_setting("rtx_shader_pack", "default")
            if enabled then
                mko.show_notification("RTX is active (pack: " .. pack .. "). Type /rtx status for options.")
            else
                mko.show_notification("RTX is disabled. Type /rtx on to enable raytracing shader packs.")
            end
        end
        return false
    end

    return false
end)
