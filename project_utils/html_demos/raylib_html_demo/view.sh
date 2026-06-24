#!/bin/bash
cd "$(dirname "$0")"
python3 -m http.server 9090 &
PID=$!
xdg-open http://localhost:9090/vault_viewer.html
read -p "Press Enter to stop server..."
kill $PID
