#!/bin/sh
# runs FalconAcmePebbleTest against a local Pebble; nothing here is fetched or committed:
#   PEBBLE_HOME   release v2.10.1+ of github.com/letsencrypt/pebble unpacked: pebble, pebble-challtestsrv,
#                 pebble.minica.pem and certs/localhost/{cert,key}.pem from the same tag
#   $1            the FalconAcmePebbleTest binary
# ports stay off 80, 443 and the game's: override them with PEBBLE_*_PORT when taken
set -eu

test_binary=${1:?usage: run.sh <FalconAcmePebbleTest>}
home=${PEBBLE_HOME:?set PEBBLE_HOME to the unpacked Pebble release}
api_port=${PEBBLE_API_PORT:-14000}
management_port=${PEBBLE_MANAGEMENT_PORT:-15000}
dns_port=${PEBBLE_DNS_PORT:-18053}
dns_management_port=${PEBBLE_DNS_MANAGEMENT_PORT:-18055}
http_port=${PEBBLE_HTTP_PORT:-19380}
tls_port=${PEBBLE_TLS_PORT:-19381}

work=$(mktemp -d)
pids=""
cleanup() {
    for pid in $pids; do
        kill "$pid" 2>/dev/null || true
    done
    rm -rf "$work"
}
trap cleanup EXIT INT TERM

cat > "$work/pebble.json" <<CONFIG
{
  "pebble": {
    "listenAddress": "127.0.0.1:$api_port",
    "managementListenAddress": "127.0.0.1:$management_port",
    "certificate": "$home/certs/localhost/cert.pem",
    "privateKey": "$home/certs/localhost/key.pem",
    "httpPort": $http_port,
    "tlsPort": $tls_port,
    "ocspResponderURL": "",
    "externalAccountBindingRequired": false,
    "retryAfter": {"authz": 1, "order": 1},
    "keyAlgorithm": "ecdsa"
  }
}
CONFIG

# every name resolves to 127.0.0.1; its own challenge servers stay off
"$home/pebble-challtestsrv" -defaultIPv4 127.0.0.1 -defaultIPv6 "" -dnsserver "127.0.0.1:$dns_port" \
    -http01 "" -https01 "" -tlsalpn01 "" -doh "" -management "127.0.0.1:$dns_management_port" \
    > "$work/challtestsrv.log" 2>&1 &
pids="$pids $!"

# Pebble's default 5% nonce rejections stay on: they exercise the badNonce retry
PEBBLE_VA_NOSLEEP=1 PEBBLE_AUTHZREUSE=0 \
    "$home/pebble" -config "$work/pebble.json" -dnsserver "127.0.0.1:$dns_port" > "$work/pebble.log" 2>&1 &
pids="$pids $!"

attempt=0
until curl -s --cacert "$home/pebble.minica.pem" "https://localhost:$api_port/dir" > /dev/null; do
    attempt=$((attempt + 1))
    if [ "$attempt" -gt 50 ]; then
        cat "$work/pebble.log"
        exit 1
    fi
    sleep 0.2
done

status=0
FALCON_PEBBLE_DIRECTORY="https://localhost:$api_port/dir" FALCON_PEBBLE_ROOTS="$home/pebble.minica.pem" \
    FALCON_PEBBLE_DOMAIN=falcon.acme.test FALCON_PEBBLE_HTTP_PORT="$http_port" FALCON_PEBBLE_TLS_PORT="$tls_port" \
    "$test_binary" || status=$?

if [ "$status" -ne 0 ]; then
    cat "$work/challtestsrv.log" "$work/pebble.log"
fi
exit "$status"
