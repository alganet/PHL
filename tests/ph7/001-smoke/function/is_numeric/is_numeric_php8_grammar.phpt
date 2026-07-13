--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_numeric() and loose comparison use PHP's numeric-string grammar
--FILE--
<?php
/* is_numeric() must accept only well-formed numeric strings (whole string):
 * leading-numeric junk, hex/binary and a dangling exponent are NOT numeric,
 * while a leading-decimal and surrounding whitespace ARE. */
foreach (["10abc", ".5", "-.5", "0x1A", "0b101", "1e3", "1e", " 12 ", "5.", "", "."] as $s) {
    echo $s === "" ? "(empty)" : $s, "=", is_numeric($s) ? "num" : "no", "\n";
}
/* PHP 8 "saner" loose comparison: a number vs a non-numeric string compares as
 * strings, so 10 == "10abc" is false; a genuine numeric string still compares
 * numerically. */
echo '10=="10abc": ', 10 == "10abc" ? "true" : "false", "\n";
echo '10=="10": ', 10 == "10" ? "true" : "false", "\n";
echo '0=="": ', 0 == "" ? "true" : "false", "\n";
echo 'max("abc",10): ', max("abc", 10), "\n";
/* A leading-decimal string is a float, not int(0): guards against the
 * numeric-branch coercion regression the tightened gate could expose. */
echo '".5"==0: ', ".5" == 0 ? "true" : "false", "\n";
echo '"-.5"==0: ', "-.5" == 0 ? "true" : "false", "\n";
echo '".5"==0.5: ', ".5" == 0.5 ? "true" : "false", "\n";
echo '".5"+0: ', ".5" + 0, "\n";
?>
--EXPECT--
10abc=no
.5=num
-.5=num
0x1A=no
0b101=no
1e3=num
1e=no
 12 =num
5.=num
(empty)=no
.=no
10=="10abc": false
10=="10": true
0=="": false
max("abc",10): abc
".5"==0: false
"-.5"==0: false
".5"==0.5: true
".5"+0: 0.5
