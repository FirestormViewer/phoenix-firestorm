// =====================================================================
//  aya_3dstream_setup.lsl
//
//  Owner-only touch UI for configuring AYAstorm 3D Stream tags on a
//  multi-prim linkset. Drop this single script into the ROOT prim only.
//
//  Touch ROOT prim  -> configure URL / Volume / range / ch
//  Touch CHILD prim -> configure that child's Volume / range / ch
//
//  Tag form written to each prim's Description (per AYAstorm spec):
//      [3dstream-stereo:{url:...}{range:N}{ch:K}{volume:V}]
//
//  - URL is shared by the linkset; only the speaker(s) carrying {url:}
//    cause AYAstorm to start/stop the stream. By convention this script
//    keeps the URL on the ROOT prim's tag.
//  - {ch:} controls how that prim consumes the stream. Channels:
//        L  R  M                       (stereo / r8)
//        FL FR C  LFE SL SR            (5.1 / r10)
//        None                          (no playback on this prim)
//  - Setting Ch=None on a speaker turns it into a source-only prim.
//    The script refuses to do this if it would leave the linkset with
//    zero speakers (which is the only real "format error" condition).
//
//  Tag-only replacement: any text in the prim Description outside the
//  [3dstream-stereo:...] tag is preserved.
//
//  After saving, AYAstorm picks up the change at the next 30-second
//  Description poll (Stream3DPollInterval). The script reminds the user.
//
//  English-only UI by design.
// =====================================================================

// ---------- Constants ----------

string  TAG_PREFIX  = "[3dstream-stereo:";   // canonical
string  ALT_PREFIX  = "[ayastream-stereo:";  // legacy alias (read-only)
string  TAG_SUFFIX  = "]";

// Fixed dialog channel; listener stays open for the script's lifetime.
integer DIALOG_CHAN = -91234567;

// Mode tokens for gMode
string  M_ROOT         = "ROOT";
string  M_CHILD        = "CHILD";
string  M_CH           = "CH";
string  M_RANGE        = "RANGE";
string  M_VOLUME       = "VOLUME";
string  M_URL          = "URL";
string  M_RANGE_CUSTOM = "RANGE_CUSTOM";
string  M_VOL_CUSTOM   = "VOL_CUSTOM";
string  M_CONFIRM_NONE = "CONFIRM_NONE";
string  M_CONFIRM_RM   = "CONFIRM_RM";

// Channel button labels (must match spec exactly: L R M FL FR C LFE SL SR)
list CH_BUTTONS = [
    "L",   "R",   "M",
    "FL",  "FR",  "C",
    "LFE", "SL",  "SR",
    "None","Back"
];

// Range presets (meters). Custom button opens textbox.
list RANGE_BUTTONS = [
    "5", "10", "20",
    "30", "50", "80",
    "Custom", "Back"
];

// Volume presets (0.0 - 1.0). Custom button opens textbox.
list VOLUME_BUTTONS = [
    "0.25", "0.50", "0.75",
    "1.00", "1.50", "2.00",
    "Custom", "Back"
];

// ---------- Globals ----------

key      gUser;       // toucher (always owner)
integer  gLink;       // link number being edited (1 = root, >=2 = child)
string   gMode;       // current dialog mode
string   gPending;    // pending value across confirm dialogs (e.g. ch name)

// ---------- Tag parsing helpers ----------

// Returns offset of TAG_PREFIX or ALT_PREFIX in s, or -1.
// Out-parameter prefix length must be inferred by the caller.
integer findTagStart(string s)
{
    integer i = llSubStringIndex(s, TAG_PREFIX);
    if (i != -1) return i;
    return llSubStringIndex(s, ALT_PREFIX);
}

integer findTagPrefixLen(string s, integer start)
{
    if (start < 0) return 0;
    string head = llGetSubString(s, start, start + llStringLength(TAG_PREFIX) - 1);
    if (head == TAG_PREFIX) return llStringLength(TAG_PREFIX);
    return llStringLength(ALT_PREFIX);
}

// Find matching closing bracket for the tag starting at 'start'. Returns -1 on failure.
integer findTagEnd(string s, integer start)
{
    integer i = llSubStringIndex(llGetSubString(s, start, -1), TAG_SUFFIX);
    if (i == -1) return -1;
    return start + i;
}

// Read all {key:value} fields inside the tag body.
// Returns strided list [key0, val0, key1, val1, ...].
list parseFields(string body)
{
    list out = [];
    integer n = llStringLength(body);
    integer i = 0;
    while (i < n)
    {
        integer ob = llSubStringIndex(llGetSubString(body, i, -1), "{");
        if (ob == -1) return out;
        ob = i + ob;
        integer cb = llSubStringIndex(llGetSubString(body, ob, -1), "}");
        if (cb == -1) return out;
        cb = ob + cb;
        string inner = llGetSubString(body, ob + 1, cb - 1);
        integer colon = llSubStringIndex(inner, ":");
        if (colon != -1)
        {
            string k = llStringTrim(llGetSubString(inner, 0, colon - 1), STRING_TRIM);
            string v = llStringTrim(llGetSubString(inner, colon + 1, -1), STRING_TRIM);
            out += [k, v];
        }
        i = cb + 1;
    }
    return out;
}

// Search only even-indexed positions so a value that happens to equal
// the searched key cannot produce a false hit.
integer findFieldIndex(list fields, string fkey)
{
    integer n = llGetListLength(fields);
    integer i;
    for (i = 0; i < n; i += 2)
    {
        if (llList2String(fields, i) == fkey) return i;
    }
    return -1;
}

string getField(list fields, string fkey)
{
    integer i = findFieldIndex(fields, fkey);
    if (i == -1) return "";
    return llList2String(fields, i + 1);
}

list setField(list fields, string fkey, string val)
{
    integer i = findFieldIndex(fields, fkey);
    if (i == -1) return fields + [fkey, val];
    return llListReplaceList(fields, [val], i + 1, i + 1);
}

list dropField(list fields, string fkey)
{
    integer i = findFieldIndex(fields, fkey);
    if (i == -1) return fields;
    return llDeleteSubList(fields, i, i + 1);
}

string buildTagBody(list fields)
{
    string body = "";
    integer n = llGetListLength(fields);
    integer i;
    // Preferred field order for readability: url, ch, range, volume, then any others.
    list order = ["url", "ch", "range", "volume"];
    list seen  = [];
    for (i = 0; i < llGetListLength(order); ++i)
    {
        string k = llList2String(order, i);
        string v = getField(fields, k);
        if (v != "")
        {
            body += "{" + k + ":" + v + "}";
            seen += [k];
        }
    }
    for (i = 0; i < n; i += 2)
    {
        string k = llList2String(fields, i);
        if (llListFindList(seen, [k]) == -1)
        {
            string v = llList2String(fields, i + 1);
            body += "{" + k + ":" + v + "}";
        }
    }
    return body;
}

// Splice new tag (or empty string) into description, preserving surrounding text.
string spliceTag(string desc, string newTag)
{
    integer start = findTagStart(desc);
    if (start == -1)
    {
        if (newTag == "") return desc;
        if (desc == "") return newTag;
        return desc + " " + newTag;
    }
    integer end = findTagEnd(desc, start);
    if (end == -1)
    {
        // malformed; replace whole description with new tag
        return newTag;
    }
    string before = "";
    if (start > 0) before = llGetSubString(desc, 0, start - 1);
    string after = "";
    if (end < llStringLength(desc) - 1) after = llGetSubString(desc, end + 1, -1);

    if (newTag == "")
    {
        // remove surrounding single space if it exists, to avoid double space
        string joined = before + after;
        // collapse a run of spaces created by removal
        while (llSubStringIndex(joined, "  ") != -1)
        {
            integer dp = llSubStringIndex(joined, "  ");
            joined = llGetSubString(joined, 0, dp - 1) + " " + llGetSubString(joined, dp + 2, -1);
        }
        return llStringTrim(joined, STRING_TRIM);
    }
    return before + newTag + after;
}

// Read the current fields list for a given link number.
// Returns [] if no tag present.
list readFields(integer link)
{
    string desc = llList2String(
        llGetLinkPrimitiveParams(link, [PRIM_DESC]),
        0
    );
    integer start = findTagStart(desc);
    if (start == -1) return [];
    integer plen = findTagPrefixLen(desc, start);
    integer end = findTagEnd(desc, start);
    if (end == -1) return [];
    string body = llGetSubString(desc, start + plen, end - 1);
    return parseFields(body);
}

// Write the given fields list back to the link's description.
// If fields is empty, removes the tag entirely.
// Returns 1 on success, 0 if the resulting description would exceed 127 bytes.
integer writeFields(integer link, list fields)
{
    string desc = llList2String(
        llGetLinkPrimitiveParams(link, [PRIM_DESC]),
        0
    );
    string newTag = "";
    if (llGetListLength(fields) > 0)
    {
        newTag = TAG_PREFIX + buildTagBody(fields) + TAG_SUFFIX;
    }
    string newDesc = spliceTag(desc, newTag);
    if (llStringLength(newDesc) > 127)
    {
        return 0;
    }
    llSetLinkPrimitiveParamsFast(link, [PRIM_DESC, newDesc]);
    return 1;
}

// Count speakers (prims with {ch:} != "" and != "None") in linkset,
// optionally excluding one link number.
integer countSpeakers(integer excludeLink)
{
    integer total = llGetNumberOfPrims();
    integer link;
    integer count = 0;
    for (link = 1; link <= total; ++link)
    {
        if (link == excludeLink) jump skip;
        list fields = readFields(link);
        string ch = getField(fields, "ch");
        if (ch != "" && llToUpper(ch) != "NONE") ++count;
        @skip;
    }
    return count;
}

// Pretty-print current values for the given link, for menu body.
string fmtCurrent(integer link)
{
    list fields = readFields(link);
    if (llGetListLength(fields) == 0)
    {
        return "(no 3dstream tag set)";
    }
    string url    = getField(fields, "url");
    string ch     = getField(fields, "ch");
    string rng    = getField(fields, "range");
    string vol    = getField(fields, "volume");
    string s = "";
    if (url != "") s += "url=" + url + "\n";
    if (ch  != "") s += "ch=" + ch + "  ";
    else           s += "ch=(none)  ";
    if (rng != "") s += "range=" + rng + "m  ";
    if (vol != "") s += "volume=" + vol;
    return s;
}

string linkLabel(integer link)
{
    if (link == LINK_ROOT || link == 1) return "ROOT prim";
    return "child prim #" + (string)link;
}

// ---------- Dialog plumbing ----------

clearMenu()
{
    gMode    = "";
    gUser    = NULL_KEY;
    gLink    = 0;
    gPending = "";
}

string getCurrentField(integer link, string fkey)
{
    list fields = readFields(link);
    string v = getField(fields, fkey);
    if (v == "") return "(none)";
    return v;
}

// ---------- Menu builders ----------

showRootMenu()
{
    gMode = M_ROOT;
    string body = "ROOT prim setup\n\n"
                + "Current:\n" + fmtCurrent(1)
                + "\n\nLinkset speakers: " + (string)countSpeakers(0)
                + "\n\nChoose a field to edit.";
    list buttons = ["Start", "Stop", "URL", "Volume", "Range", "Ch", "Remove Tag", "Close"];
    llDialog(gUser, body, buttons, DIALOG_CHAN);
}

showChildMenu()
{
    gMode = M_CHILD;
    string body = "Child prim setup (link #" + (string)gLink + ")\n\n"
                + "Current:\n" + fmtCurrent(gLink)
                + "\n\nChoose a field to edit.";
    list buttons = ["Volume", "Range", "Ch", "Remove Tag", "Close"];
    llDialog(gUser, body, buttons, DIALOG_CHAN);
}

showCh()
{
    gMode = M_CH;
    string body = "Select channel for " + linkLabel(gLink) + "\n\n"
                + "L/R/M: stereo or mid-mix\n"
                + "FL/FR/C/LFE/SL/SR: 5.1 placement\n"
                + "None: no playback on this prim (source-only)";
    llDialog(gUser, body, CH_BUTTONS, DIALOG_CHAN);
}

showRange()
{
    gMode = M_RANGE;
    string body = "Select range (meters) for " + linkLabel(gLink) + "\n"
                + "Current: " + getCurrentField(gLink, "range") + "m";
    llDialog(gUser, body, RANGE_BUTTONS, DIALOG_CHAN);
}

showVolume()
{
    gMode = M_VOLUME;
    string body = "Select volume for " + linkLabel(gLink) + "\n"
                + "Current: " + getCurrentField(gLink, "volume");
    llDialog(gUser, body, VOLUME_BUTTONS, DIALOG_CHAN);
}

showUrl()
{
    gMode = M_URL;
    string current = getCurrentField(1, "url");
    if (current == "") current = "(empty)";
    llTextBox(gUser,
        "Enter stream URL (http:// or https://). Empty input cancels.\n\nCurrent: " + current,
        DIALOG_CHAN);
}

showRangeCustom()
{
    gMode = M_RANGE_CUSTOM;
    llTextBox(gUser,
        "Enter custom range in meters (positive number). Empty input cancels.",
        DIALOG_CHAN);
}

showVolumeCustom()
{
    gMode = M_VOL_CUSTOM;
    llTextBox(gUser,
        "Enter custom volume (0.0 - 4.0). Empty input cancels.",
        DIALOG_CHAN);
}

// ---------- Action handlers ----------

notifySaved()
{
    llRegionSayTo(gUser, 0,
        "Saved. AYAstorm re-reads tags every 30 seconds, "
        + "so playback may take up to half a minute to update.");
}

applyField(integer link, string fkey, string val)
{
    list fields = readFields(link);
    if (val == "") fields = dropField(fields, fkey);
    else           fields = setField(fields, fkey, val);
    if (writeFields(link, fields) == 0)
    {
        llRegionSayTo(gUser, 0,
            "Cannot save: resulting Description would exceed 127 bytes. "
            + "Try a shorter URL or remove other text from the Description.");
        return;
    }
    notifySaved();
}

handleChSelect(string ch)
{
    if (ch == "Back")
    {
        if (gLink == 1) showRootMenu();
        else            showChildMenu();
        return;
    }
    if (ch == "None")
    {
        // Safety: ensure at least one other speaker remains.
        if (countSpeakers(gLink) == 0)
        {
            gMode = M_CONFIRM_NONE;
            gPending = "None";
            llDialog(gUser,
                "WARNING: setting Ch=None on this prim would leave the "
              + "linkset with zero speakers, which AYAstorm treats as a "
              + "format error.\n\n"
              + "Add a speaker on another prim first, or cancel.",
                ["Cancel"], DIALOG_CHAN);
            return;
        }
        list fields = readFields(gLink);
        fields = dropField(fields, "ch");
        if (writeFields(gLink, fields) == 0)
        {
            llRegionSayTo(gUser, 0, "Save failed (description too long).");
            clearMenu();
            return;
        }
        notifySaved();
        clearMenu();
        return;
    }
    applyField(gLink, "ch", ch);
    clearMenu();
}

handleStop()
{
    list fields = readFields(1);
    string url = getField(fields, "url");
    if (url == "") { clearMenu(); return; }
    fields = dropField(fields, "url");
    fields = setField(fields, "urlsave", url);
    if (writeFields(1, fields) == 0)
    {
        llRegionSayTo(gUser, 0, "Save failed (description too long).");
        clearMenu();
        return;
    }
    notifySaved();
    clearMenu();
}

handleStart()
{
    list fields = readFields(1);
    string saved = getField(fields, "urlsave");
    if (saved == "")
    {
        // No saved URL — prompt for one.
        showUrl();
        return;
    }
    fields = dropField(fields, "urlsave");
    fields = setField(fields, "url", saved);
    if (writeFields(1, fields) == 0)
    {
        llRegionSayTo(gUser, 0, "Save failed (description too long).");
        clearMenu();
        return;
    }
    notifySaved();
    clearMenu();
}

handleRange(string label)
{
    if (label == "Back")
    {
        if (gLink == 1) showRootMenu();
        else            showChildMenu();
        return;
    }
    if (label == "Custom")
    {
        showRangeCustom();
        return;
    }
    applyField(gLink, "range", label);
    clearMenu();
}

handleVolume(string label)
{
    if (label == "Back")
    {
        if (gLink == 1) showRootMenu();
        else            showChildMenu();
        return;
    }
    if (label == "Custom")
    {
        showVolumeCustom();
        return;
    }
    applyField(gLink, "volume", label);
    clearMenu();
}

handleRangeCustom(string text)
{
    text = llStringTrim(text, STRING_TRIM);
    if (text == "") { clearMenu(); return; }
    float v = (float)text;
    if (v <= 0.0)
    {
        llRegionSayTo(gUser, 0, "Range must be a positive number.");
        clearMenu();
        return;
    }
    applyField(gLink, "range", text);
    clearMenu();
}

handleVolumeCustom(string text)
{
    text = llStringTrim(text, STRING_TRIM);
    if (text == "") { clearMenu(); return; }
    float v = (float)text;
    if (v < 0.0 || v > 4.0)
    {
        llRegionSayTo(gUser, 0, "Volume must be between 0.0 and 4.0.");
        clearMenu();
        return;
    }
    applyField(gLink, "volume", text);
    clearMenu();
}

handleUrl(string text)
{
    text = llStringTrim(text, STRING_TRIM);
    if (text == "") { clearMenu(); return; }
    if (llSubStringIndex(text, "http://") != 0
     && llSubStringIndex(text, "https://") != 0)
    {
        llRegionSayTo(gUser, 0, "URL must start with http:// or https://");
        clearMenu();
        return;
    }
    applyField(1, "url", text);
    clearMenu();
}

handleRemoveTag()
{
    // Removing a speaker leaves the linkset with no playback prim?
    list fields = readFields(gLink);
    string ch = getField(fields, "ch");
    integer isSpeaker = (ch != "" && llToUpper(ch) != "NONE");
    if (isSpeaker && countSpeakers(gLink) == 0)
    {
        gMode = M_CONFIRM_RM;
        llDialog(gUser,
            "WARNING: this prim is currently the only speaker in the "
          + "linkset. Removing its tag would leave AYAstorm with nothing "
          + "to play (format error).\n\n"
          + "Set up another speaker first, or cancel.",
            ["Cancel"], DIALOG_CHAN);
        return;
    }
    if (writeFields(gLink, []) == 0)
    {
        llRegionSayTo(gUser, 0, "Save failed (description too long).");
        clearMenu();
        return;
    }
    llRegionSayTo(gUser, 0,
        "Tag removed. AYAstorm will stop using this prim within ~30s.");
    clearMenu();
}

// ---------- State ----------

default
{
    state_entry()
    {
        clearMenu();
        llListen(DIALOG_CHAN, "", NULL_KEY, "");
    }

    on_rez(integer p)
    {
        clearMenu();
    }

    touch_start(integer n)
    {
        gUser = llDetectedKey(0);
        integer link = llDetectedLinkNumber(0);
        if (link == 0) link = 1;   // un-linked single prim
        gLink = link;

        if (link == 1) showRootMenu();
        else           showChildMenu();
    }

    listen(integer chan, string name, key id, string msg)
    {
        if (id != gUser) return;

        // ---- Root menu ----
        if (gMode == M_ROOT)
        {
            if (msg == "Close")      { clearMenu(); return; }
            if (msg == "Start")      { handleStart(); return; }
            if (msg == "Stop")       { handleStop(); return; }
            if (msg == "URL")        { showUrl(); return; }
            if (msg == "Volume")     { showVolume(); return; }
            if (msg == "Range")      { showRange(); return; }
            if (msg == "Ch")         { showCh(); return; }
            if (msg == "Remove Tag") { handleRemoveTag(); return; }
            return;
        }

        // ---- Child menu ----
        if (gMode == M_CHILD)
        {
            if (msg == "Close")      { clearMenu(); return; }
            if (msg == "Volume")     { showVolume(); return; }
            if (msg == "Range")      { showRange(); return; }
            if (msg == "Ch")         { showCh(); return; }
            if (msg == "Remove Tag") { handleRemoveTag(); return; }
            return;
        }

        // ---- Sub-menus ----
        if (gMode == M_CH)         { handleChSelect(msg);     return; }
        if (gMode == M_RANGE)      { handleRange(msg);        return; }
        if (gMode == M_VOLUME)     { handleVolume(msg);       return; }
        if (gMode == M_RANGE_CUSTOM) { handleRangeCustom(msg); return; }
        if (gMode == M_VOL_CUSTOM)   { handleVolumeCustom(msg); return; }
        if (gMode == M_URL)        { handleUrl(msg);          return; }

        // ---- Confirm dialogs ----
        if (gMode == M_CONFIRM_NONE) { clearMenu(); return; }
        if (gMode == M_CONFIRM_RM)   { clearMenu(); return; }
    }

    changed(integer change)
    {
        if (change & CHANGED_OWNER) llResetScript();
    }
}
