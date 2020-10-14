#!/usr/bin/env bash
if [ $# -eq 0 ]
then
    echo "USAGE: $0 <OS|SL>"
    exit 1
fi

if [ $1 == "SL" ] || [ $1 == "OS" ]
then 
    [ $1 == "SL" ] && l="OS" || l="SL"
else
    if [ $1 == "test" ] || [ $1 == "?" ]
    then
        if [ -d "./build-vc150-64" ] 
        then
            c="Unknown"
            if [ ! -d "./build-vc150-64-SL" ] && [ -d "./build-vc150-64-OS" ]
            then
                c="SL"
            fi
            if [ -d "./build-vc150-64-SL" ] && [ ! -d "./build-vc150-64-OS" ]
            then
                c="OS"
            fi
            echo "Current build is for $c"
        else
            echo "No build configured"
        fi
        exit 0
    else
        echo "Invalid target $1"
        echo "USAGE: $0 <OS|SL>"
        exit 2
    fi
fi
t=$1

if [ -d "./build-vc150-64" ] 
then
    if [ ! -d "./build-vc150-64-$t" ] 
    then
        # live folder exists
        # target does not so we are probably good to go
        echo "$t already setup"
        exit 0
    else
        # target folder exists so probably means that the setup is for the alternate
        if [ ! -d "./build-vc150-64-$l" ] 
        then
            # alternate folder not found so we rename the live target back to this
            echo "Currently setup for $l. Renaming default back to $l"
            `mv ./build-vc150-64 ./build-vc150-64-$l`
        else
        # no LAST and no TARGET exists so this is not in the right state
            echo "Not setup for multi-target"
            exit 3
        fi
    fi
fi

if [ -d "./build-vc150-64-$t" ] 
then
    # live folder exists
    # target does not so we are probably good to go
    echo "renaming $t to default"
    `mv ./build-vc150-64-$t ./build-vc150-64`
else
    echo "Not setup for target $t"
    exit 4
fi

