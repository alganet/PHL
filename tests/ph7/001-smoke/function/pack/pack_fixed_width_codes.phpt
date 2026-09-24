--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
pack() writes each fixed-width code at its own width and byte order
--FILE--
<?php
/* The eighteen codes that turn a NUMBER into a fixed run of bytes. Three groups
 * (i/I, f/g/G, d/e/E) are sized by the host's own C types; the rest are fixed by
 * the format. Asserted as hex, because the bytes are the point. */
function pack_row(string $fmt, ...$values): void {
    echo $fmt, '=', bin2hex(pack($fmt, ...$values)), "\n";
}
echo "## widths\n";
pack_row('c', 1);
pack_row('C', 1);
pack_row('s', 1);
pack_row('S', 1);
pack_row('n', 1);
pack_row('v', 1);
pack_row('i', 1);
pack_row('I', 1);
pack_row('l', 1);
pack_row('L', 1);
pack_row('N', 1);
pack_row('V', 1);
pack_row('q', 1);
pack_row('Q', 1);
pack_row('J', 1);
pack_row('P', 1);

echo "## byte order: the same value through every explicit width\n";
pack_row('n', 0x1234);
pack_row('v', 0x1234);
pack_row('N', 0x11223344);
pack_row('V', 0x11223344);
pack_row('J', 0x1122334455667788);
pack_row('P', 0x1122334455667788);

echo "## only the low bytes of the value reach a narrow field\n";
pack_row('C', 255);
pack_row('C', 256);
pack_row('C', 257);
pack_row('C', -1);
pack_row('n', -1);
pack_row('n', 65536 + 5);
pack_row('N', -1);
pack_row('J', -1);
pack_row('J', PHP_INT_MIN);
pack_row('P', PHP_INT_MAX);

echo "## floats travel as their IEEE-754 bit pattern\n";
pack_row('G', 1.5);
pack_row('g', 1.5);
pack_row('E', 1.5);
pack_row('e', 1.5);
pack_row('G', -0.0);
pack_row('E', -0.0);
pack_row('E', 3.141592653589793);
pack_row('G', 1e30);
pack_row('E', 1e30);
/* f and d are the machine's own order, so each must agree with whichever half
 * of its explicit pair the host uses -- and so must s against n/v. */
var_dump(pack('f', 1.5) === pack('g', 1.5) || pack('f', 1.5) === pack('G', 1.5));
var_dump(pack('d', 1.5) === pack('e', 1.5) || pack('d', 1.5) === pack('E', 1.5));
var_dump(pack('s', 1) === pack('v', 1) || pack('s', 1) === pack('n', 1));

echo "## a repeater takes that many arguments; '*' takes the rest\n";
pack_row('N2', 1, 2);
pack_row('N*', 1, 2, 3);
pack_row('C*');
pack_row('N0');
pack_row('nvC', 0x0102, 0x0304, 5);

echo "## values are coerced the way any int/float parameter is\n";
pack_row('N', true);
pack_row('N', false);
pack_row('N', null);
pack_row('N', '12');
pack_row('N', 1.9);
pack_row('N', -1.9);
pack_row('E', '1.5');
pack_row('E', 3);
?>
--EXPECT--
## widths
c=01
C=01
s=0100
S=0100
n=0001
v=0100
i=01000000
I=01000000
l=01000000
L=01000000
N=00000001
V=01000000
q=0100000000000000
Q=0100000000000000
J=0000000000000001
P=0100000000000000
## byte order: the same value through every explicit width
n=1234
v=3412
N=11223344
V=44332211
J=1122334455667788
P=8877665544332211
## only the low bytes of the value reach a narrow field
C=ff
C=00
C=01
C=ff
n=ffff
n=0005
N=ffffffff
J=ffffffffffffffff
J=8000000000000000
P=ffffffffffffff7f
## floats travel as their IEEE-754 bit pattern
G=3fc00000
g=0000c03f
E=3ff8000000000000
e=000000000000f83f
G=80000000
E=8000000000000000
E=400921fb54442d18
G=7149f2ca
E=46293e5939a08cea
bool(true)
bool(true)
bool(true)
## a repeater takes that many arguments; '*' takes the rest
N2=0000000100000002
N*=000000010000000200000003
C*=
N0=
nvC=0102040305
## values are coerced the way any int/float parameter is
N=00000001
N=00000000
N=00000000
N=0000000c
N=00000001
N=ffffffff
E=3ff8000000000000
E=4008000000000000
