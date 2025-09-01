#!/usr/bin/env sh
set -eu

: "${BACKEND_URL:=http://192.168.4.1}"

echo "Using BACKEND_URL=$BACKEND_URL"
envsubst '$BACKEND_URL' < /etc/nginx/templates/nginx.conf.template > /etc/nginx/conf.d/default.conf
exec nginx -g 'daemon off;'

