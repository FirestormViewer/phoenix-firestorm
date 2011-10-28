#!/bin/bash

# Send a URL of the form secondlife://... to the viewer.
#

URL="$1"

if [ -z "$URL" ]; then
    #echo Usage: $0 secondlife://...
    echo "Usage: $0 [ secondlife://  | hop:// ] ..."
    exit
fi

RUN_PATH=`dirname "$0" || echo .`
#cd "${RUN_PATH}/.."
ch "${RUN_PATH}"

#exec ./firestorm -url \'"${URL}"\'
if [ `pidof do-not-directly-run-firestorm-bin` ]; then
	exec dbus-send --type=method_call --dest=com.secondlife.ViewerAppAPIService /com/secondlife/ViewerAppAPI com.secondlife.ViewerAppAPI.GoSLURL string:"$1"
else
	exec ../firestorm -url \'"${URL}"\'
fi
`

