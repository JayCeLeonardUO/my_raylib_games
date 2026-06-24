#!/bin/bash
PORT=${1:-8080}
DIR="$(cd "$(dirname "$0")" && pwd)"
FILE="$(basename "$0")"
echo "Serving at http://localhost:$PORT/$FILE"
python3 -c "
import http.server, os, threading, signal
os.chdir('$DIR')
class H(http.server.SimpleHTTPRequestHandler):
    def do_POST(self):
        if self.path == '/shutdown':
            self.send_response(200)
            self.end_headers()
            threading.Thread(target=self.server.shutdown).start()
        else:
            self.send_error(404)
    def log_message(self, *a): pass
s = http.server.HTTPServer(('', $PORT), H)
s.serve_forever()
" &
PID=$!
trap "kill $PID 2>/dev/null" EXIT
sleep 1
xdg-open "http://localhost:$PORT/$FILE" 2>/dev/null || open "http://localhost:$PORT/$FILE" 2>/dev/null
wait "$PID"
echo "Server stopped."
exit 0
