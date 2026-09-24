--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: a pack() repeater no int can hold is refused, where php wraps it
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip php parses the repeater with atoi(), whose answer past INT_MAX is the C library's: glibc truncates the parsed long into an int, so pack('a99999999999','xy') allocates the 1215752191 bytes the wrap lands on and pack('x2147483648') wraps NEGATIVE and is read as '*'. PHL refuses the class instead, so this half cannot be cross-engine.";
}
?>
--FILE--
<?php
/* A repeat count is a C int everywhere in this family -- php's own output
 * position is one, and it refuses an output that would overflow it with
 * `Type %c: integer overflow in format string`. A COUNT that already does not
 * fit takes the same refusal here, one step earlier, rather than being wrapped
 * into whatever int the low 32 bits happen to spell. */
foreach ([
    'a99999999999',
    'x2147483648',
    'x4294967296',
    '@99999999999',
    'X99999999999',
    'N99999999999',
    'H99999999999',
    'c18446744073709551617',
] as $fmt) {
    try {
        $r = pack($fmt, 'xy');
        echo $fmt, ' -> ', strlen($r), " bytes\n";
    } catch (\Throwable $e) {
        echo $fmt, ' -> ', get_class($e), ': ', $e->getMessage(), "\n";
    }
}
/* A count that DOES fit is still read as a count -- the refusal is about the
 * parse, not about the format being long-winded. */
var_dump(strlen(pack('a0002000', 'xy')));
?>
--EXPECT--
a99999999999 -> ValueError: Type a: integer overflow in format string
x2147483648 -> ValueError: Type x: integer overflow in format string
x4294967296 -> ValueError: Type x: integer overflow in format string
@99999999999 -> ValueError: Type @: integer overflow in format string
X99999999999 -> ValueError: Type X: integer overflow in format string
N99999999999 -> ValueError: Type N: integer overflow in format string
H99999999999 -> ValueError: Type H: integer overflow in format string
c18446744073709551617 -> ValueError: Type c: integer overflow in format string
int(2000)
