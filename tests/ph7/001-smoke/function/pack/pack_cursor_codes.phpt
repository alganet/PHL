--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
pack()'s x/X/@ move the output cursor, and the result is where it stopped
--FILE--
<?php
/* x, X and @ take no argument at all: x writes NUL bytes, X backs the cursor up
 * over bytes already written, and @ is an absolute position rather than a count.
 * The RESULT is as long as the cursor was when the format ran out -- which is
 * not the same as the longest it ever got. */
function pack_cur(string $fmt, ...$values): void {
    echo $fmt, '=', bin2hex(pack($fmt, ...$values)), "\n";
}
echo "## x writes NULs, @ fills up to a position\n";
pack_cur('x');
pack_cur('x3');
pack_cur('x0');
pack_cur('@0');
pack_cur('@4');
pack_cur('C@4C', 0x41, 0x42);
pack_cur('C@2', 0x41);
pack_cur('@2C', 0x41);

echo "## X backs up, and a later write lands on top of what was there\n";
pack_cur('CCX', 0x41, 0x42);
pack_cur('CCXC', 0x41, 0x42, 0x43);
pack_cur('CCX2C', 0x41, 0x42, 0x43);
pack_cur('NX2n', 0x11223344, 0xaabb);
pack_cur('a3X3a2', 'abc', 'z');

echo "## the answer is the LAST position, not the highest\n";
pack_cur('x8X4');
pack_cur('@8X4');
pack_cur('x8@2');
pack_cur('C2X1', 0x41, 0x42);
pack_cur('@0C', 0x41);
pack_cur('C@0C', 0x41, 0x42);

echo "## @ never shrinks the buffer it already filled\n";
pack_cur('x4@2C', 0x41);
pack_cur('C@0', 0x41);
?>
--EXPECT--
## x writes NULs, @ fills up to a position
x=00
x3=000000
x0=
@0=
@4=00000000
C@4C=4100000042
C@2=4100
@2C=000041
## X backs up, and a later write lands on top of what was there
CCX=41
CCXC=4143
CCX2C=43
NX2n=1122aabb
a3X3a2=7a00
## the answer is the LAST position, not the highest
x8X4=00000000
@8X4=00000000
x8@2=0000
C2X1=41
@0C=41
C@0C=42
## @ never shrinks the buffer it already filled
x4@2C=000041
C@0=
