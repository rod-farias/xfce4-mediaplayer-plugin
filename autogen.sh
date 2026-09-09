#!/bin/sh
set -e

srcdir=$(dirname "$0")
test -z "$srcdir" && srcdir=.

cd "$srcdir"

autoreconf --force --install --verbose

cd - >/dev/null

echo "Now run: $srcdir/configure && make"
