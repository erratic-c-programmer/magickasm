#! /bin/sh
magicc/magicc.py -o "$(dirname "$1")"/"$(basename "$1" '.ma')" "$1"
vmagick/vmagick "$(dirname "$1")"/"$(basename "$1" '.ma')"
