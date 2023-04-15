#! /bin/sh
magicc/magicc.py "$1" "$(dirname $1)"/"$(basename $1 '.ma')"
vmagick/vmagick "$(dirname $1)"/"$(basename $1 '.ma')"
