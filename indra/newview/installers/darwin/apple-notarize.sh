#!/bin/bash
set -euo pipefail

if [[ ${SKIP_NOTARIZATION:-false} == "true" ]]; then
    if [[ ${CI_SIGNING_REQUIRED:-false} == "true" ]]; then
        echo "Required notarization cannot be skipped" >&2
        exit 1
    fi
    echo "Skipping notarization"
    exit 0
fi

CONFIG_FILE="$build_secrets_checkout/code-signing-osx/notarize_creds.sh"
if [[ ! -f "$CONFIG_FILE" ]]; then
    echo "No config file found - check notarize_creds is present in the secrets" >&2
    exit 1
fi
source "$CONFIG_FILE"
app_file="$1"
zip_file="${app_file%.app}.zip"
result_file=$(mktemp)
trap 'rm -f "$result_file" "$zip_file"' EXIT

ditto -c -k --keepParent "$app_file" "$zip_file"
# A submission ID is also returned for rejected submissions. Check both the
# command's status and the structured acceptance result before stapling.
xcrun notarytool submit "$zip_file" \
    --apple-id "$USERNAME" --password "$PASSWORD" --team-id "$ASC_PROVIDER" \
    --wait --output-format json > "$result_file"
"${PYTHON:-python3}" - "$result_file" <<'PY'
import json
import sys
with open(sys.argv[1], encoding='utf-8') as stream:
    result = json.load(stream)
print('Notarization status:', result.get('status', 'missing'))
print('Submission ID:', result.get('id', 'missing'))
if result.get('status') != 'Accepted':
    sys.exit('Notarization was not accepted')
PY

for attempt in {1..5}; do
    echo "Running stapler (attempt $attempt of 5)..."
    if xcrun stapler staple "$app_file"; then
        exit 0
    fi
    if [[ $attempt -lt 5 ]]; then
        sleep 5
    fi
done
echo "Stapling failed after 5 attempts" >&2
exit 65
