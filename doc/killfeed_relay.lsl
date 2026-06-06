// Soapstorm Kill Feed relay (standalone fallback)
//
// NOTE: you normally do NOT need this script. The Firestorm LSL bridge
// (bridge version 2.30+) feeds the Kill Feed automatically while the
// floater is open. This standalone relay exists only for users who run
// with the LSL bridge disabled.
//
// Wear this script in any attachment (a small invisible prim is fine).
// It listens to the region combat log (Linden Damage 2 / Combat 2.0) and
// forwards DEATH events to your own viewer, which displays them under
// World > Kill Feed.
//
// Notes:
// - The combat log is broadcast region-wide on COMBAT_CHANNEL and system
//   events always come from COMBAT_LOG_ID, which scripts cannot forge.
// - Only DEATH events are forwarded; DAMAGE events are deliberately
//   dropped here to keep this script's event queue healthy in large
//   battles (the combat channel is extremely busy under fire).
// - llOwnerSay is only visible to the wearer. On viewers without the
//   Kill Feed feature the forwarded lines appear as raw text in local
//   chat; detach the relay if that bothers you there.

forward_death(string ev)
{
    // Resolve the weapon name while the objects still exist. Projectiles
    // usually de-rez on impact, so fall back to the rezzing object (the
    // gun), which normally persists as an attachment on the killer.
    string wname = "";
    key source = (key)llJsonGetValue(ev, ["source"]);
    if (source)
    {
        list d = llGetObjectDetails(source, [OBJECT_NAME]);
        if (d != []) wname = llList2String(d, 0);
    }
    if (wname == "")
    {
        key rezzer = (key)llJsonGetValue(ev, ["rezzer"]);
        if (rezzer)
        {
            list d = llGetObjectDetails(rezzer, [OBJECT_NAME]);
            if (d != []) wname = llList2String(d, 0);
        }
    }
    if (wname != "") ev = llJsonSetValue(ev, ["weapon_name"], wname);

    llOwnerSay("SSKF|" + ev);
}

default
{
    state_entry()
    {
        llListen(COMBAT_CHANNEL, "", COMBAT_LOG_ID, "");
    }

    listen(integer channel, string name, key id, string message)
    {
        // The simulator may batch several events into one JSON array.
        if (llGetSubString(message, 0, 0) == "[")
        {
            integer i = 0;
            string ev = llJsonGetValue(message, [i]);
            while (ev != JSON_INVALID)
            {
                if (llJsonGetValue(ev, ["event"]) == "DEATH")
                {
                    forward_death(ev);
                }
                ev = llJsonGetValue(message, [++i]);
            }
        }
        else if (llJsonGetValue(message, ["event"]) == "DEATH")
        {
            forward_death(message);
        }
    }
}
