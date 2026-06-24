#!/bin/bash
# Usage: postbuild.sh <header.sh> <built.html> <output.html>
cat "$1" "$2" > "$3"
chmod +x "$3"
