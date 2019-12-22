#!/bin/bash

try() {
    expected="$1"
    input="$2"

    ./9cc "$input" > tmp.s
    gcc -o tmp tmp.s
    ./tmp
    actual="$?"

    if [ "$actual" = "$expected" ]; then
        echo "$input => $actual"
    else
        echo "$input => $expected expected, but got $actual"
        exit 1
    fi
}

try 0 "0;"
try 42 "42;"
try 21 "5+20-4;"
try 198 "200-5+3;"
try 41 " 12 + 34 - 5 ;"
try 100 "11+  120 -2-	30+1;"
try 47 '5+6*7;'
try 15 '5*(9-6);'
try 4 '(3+5)/2;'
try 10 "-10+20;"
try 0 "10 + 1 < 10 -1;"
try 0 "10 + 1 <= 10 -1;"
try 1 "10 + 1 > 10 -1;"
try 1 "10 + 1 >= 10 -1;"
try 1 "10 + 1 == 10 -1 + 2;"
try 1 "10 != 3;"
try 3 "a=3;"
try 10 "a=3;7+a;"

echo OK
