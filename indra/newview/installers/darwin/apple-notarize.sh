#!/bin/sh
if [[ $SKIP_NOTARIZATION == "true" ]]; then
    echo "Skipping notarization"
    exit 0
fi

CONFIG_FILE="$build_secrets_checkout/code-signing-osx/notarize_creds.sh"
if [[ -f "$CONFIG_FILE" ]]; then
    source "$CONFIG_FILE"
    app_file="$1"
    zip_file=${app_file/app/zip}
    ditto -c -k --keepParent "$app_file" "$zip_file"
    if [[ -f "$zip_file" ]]; then
        # res=$(xcrun notarytool store-credentials \
        #                         viewer.profile \
        #                         --verbose 2>*1)
        # echo $res
        res=$(xcrun notarytool submit "$zip_file" \
                                --apple-id $USERNAME \
                                --password $PASSWORD \
                                --team-id $ASC_PROVIDER \
                                --wait 2>&1)
        echo "Notarytool submit:"
        echo $res
        
        if [[ "$res" =~ id:\ ([^[:space:]]+) ]]; then
            match="${BASH_REMATCH[1]}"
            echo "Notarized with id: [$match]"
        else
            echo "No match found"
        fi

        # if [[ ! $match -eq 0 ]]; then
        echo "Running Stapler"
        xcrun stapler staple "$app_file"
        # Delete the zip file to stop it being packed in the dmg
        rm -f "$zip_file"
        if [[ $? -eq 0 ]]; then
            echo "$zip_file deleted successfully."
        else
            echo "Failed to delete $zip_file"
        fi
        exit 0
        # else
            # echo "Notarization error"
            # exit 1
        # fi
    else
        echo "Notarization error: ditto failed"
        exit 1
    fi
else
    echo "No config file found - check notarize_creds is present in the secrets"
    exit 1
fi
