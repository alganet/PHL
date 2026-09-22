--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unserialize() saturates an out-of-range i: token and reports it, once per token
--FILE--
<?php
/* php reads `i:<int>;` with a clamping reader: a digit run past the signed 64-bit
 * range comes back as PHP_INT_MAX (or PHP_INT_MIN) behind the warning
 * `unserialize(): Numerical result out of range`, once for each such token.
 * The magnitude used to be accumulated with unchecked wraparound, so the value
 * had no relation to the payload at all. */
set_error_handler(function ($n, $s) { echo "  W: $s\n"; return true; });
foreach ([
    'i:9223372036854775808;',
    'i:-9223372036854775809;',
    'i:18446744073709551616;',
    'i:99999999999999999999999;',
    'i:+9223372036854775808;',
    'i:00009223372036854775808;',
    /* One warning per token, and the KEY position reads through the same one. */
    'a:2:{i:0;i:99999999999999999999;i:1;i:99999999999999999998;}',
    'a:1:{i:99999999999999999999;s:1:"x";}',
    /* In range: silent, exact, both boundaries included. */
    'i:9223372036854775807;',
    'i:-9223372036854775808;',
    'i:-0;',
    'i:42;',
    /* Malformed stays php's offset failure, with no range warning of its own. */
    'i:99999999999999999999X',
    'i: 1;',
    'i:1e3;',
] as $s) {
    echo $s, "\n";
    var_dump(unserialize($s));
}
/* A round trip through the boundary values is unaffected. */
var_dump(unserialize(serialize(PHP_INT_MIN)) === PHP_INT_MIN);
var_dump(unserialize(serialize(PHP_INT_MAX)) === PHP_INT_MAX);
restore_error_handler();
?>
--EXPECT--
i:9223372036854775808;
  W: unserialize(): Numerical result out of range
int(9223372036854775807)
i:-9223372036854775809;
  W: unserialize(): Numerical result out of range
int(-9223372036854775808)
i:18446744073709551616;
  W: unserialize(): Numerical result out of range
int(9223372036854775807)
i:99999999999999999999999;
  W: unserialize(): Numerical result out of range
int(9223372036854775807)
i:+9223372036854775808;
  W: unserialize(): Numerical result out of range
int(9223372036854775807)
i:00009223372036854775808;
  W: unserialize(): Numerical result out of range
int(9223372036854775807)
a:2:{i:0;i:99999999999999999999;i:1;i:99999999999999999998;}
  W: unserialize(): Numerical result out of range
  W: unserialize(): Numerical result out of range
array(2) {
  [0]=>
  int(9223372036854775807)
  [1]=>
  int(9223372036854775807)
}
a:1:{i:99999999999999999999;s:1:"x";}
  W: unserialize(): Numerical result out of range
array(1) {
  [9223372036854775807]=>
  string(1) "x"
}
i:9223372036854775807;
int(9223372036854775807)
i:-9223372036854775808;
int(-9223372036854775808)
i:-0;
int(0)
i:42;
int(42)
i:99999999999999999999X
  W: unserialize(): Error at offset 0 of 23 bytes
bool(false)
i: 1;
  W: unserialize(): Error at offset 0 of 5 bytes
bool(false)
i:1e3;
  W: unserialize(): Error at offset 0 of 6 bytes
bool(false)
bool(true)
bool(true)
--CLEAN--
<?php
