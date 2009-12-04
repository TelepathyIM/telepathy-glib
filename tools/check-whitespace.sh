#!/bin/sh

fail=0

if grep -n ' $' "$@"
then
  echo "^^^ The above files contain unwanted trailing spaces"
  fail=1
fi

if grep -n '	$' "$@"
then
  echo "^^^ The above files contain unwanted trailing tabs"
  fail=1
fi

# TODO: enable tab checking once all Empathy switched to TP coding style
#if grep -n '	' "$@"
#then
#  echo "^^^ The above files contain tabs"
#  fail=1
#fi

exit $fail
