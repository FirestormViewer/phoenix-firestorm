#!/bin/bash

# Send a secondlife:// or hop:// URL to the running viewer, or start it.
URL="${1:-}"
if [[ -z "$URL" ]]; then
    echo "Usage: $0 [ secondlife:// | hop:// ] ..." >&2
    exit 1
fi

SCRIPTSRC=$(readlink -f "$0") || exit 1
RUN_PATH=$(dirname "$SCRIPTSRC")
cd "$RUN_PATH" || exit 1

if pidof do-not-directly-run-firestorm-bin >/dev/null 2>&1; then
    exec dbus-send --type=method_call --dest=com.secondlife.ViewerAppAPIService \
        /com/secondlife/ViewerAppAPI com.secondlife.ViewerAppAPI.GoSLURL "string:$URL"
else
    exec ../firestorm -url "$URL"
fi
