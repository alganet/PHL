--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unpack() names each entry the way php names it, and reads php's widths back
--FILE--
<?php
/* The format is a '/'-separated list of NAMED entries: a code, an optional
 * repeater, then a name that runs to the next '/'. What the name becomes is
 * three rules -- no name at all is keyed by POSITION, one repetition takes the
 * name as written, and a repeated one has its 1-based index appended. */
function unpack_row(string $fmt, string $data, int $offset = 0): void {
    $r = unpack($fmt, $data, $offset);
    echo $fmt, ' => ', json_encode($r === false ? false
        : array_map(fn($v) => is_string($v) ? bin2hex($v) : $v, $r)), "\n";
}
echo "## the three naming rules\n";
unpack_row('N', "\x01\x02\x03\x04");
unpack_row('Nfoo', "\x01\x02\x03\x04");
unpack_row('C*', 'abc');
unpack_row('C*c', 'abc');
unpack_row('C3n', 'abc');
unpack_row('C1n', 'abc');
unpack_row('Nfoo/Cbar', "\x01\x02\x03\x04\x09");
unpack_row('Ca/Cb', "\x05\x06");
/* Everything up to the '/' is the name, spaces included -- and a name that
 * spells an integer becomes an integer key, as any php array key does. */
unpack_row('CX C', "\x05");
unpack_row('a*1', 'abc');
unpack_row('C 2', "\x05");

echo "## the fixed-width codes read back what pack() wrote\n";
unpack_row('cs', "\xff");
unpack_row('Cs', "\xff");
unpack_row('ns', "\x12\x34");
unpack_row('vs', "\x12\x34");
unpack_row('Ns', "\x11\x22\x33\x44");
unpack_row('Vs', "\x11\x22\x33\x44");
unpack_row('Js', "\xff\xff\xff\xff\xff\xff\xff\xff");
unpack_row('Ps', "\x01\x00\x00\x00\x00\x00\x00\x00");
unpack_row('qs', "\xff\xff\xff\xff\xff\xff\xff\xff");
unpack_row('Qs', "\xff\xff\xff\xff\xff\xff\xff\xff");
unpack_row('ls', "\xff\xff\xff\xff");
unpack_row('Ls', "\xff\xff\xff\xff");
unpack_row('ss', "\xff\xff");
unpack_row('Ss', "\xff\xff");
unpack_row('is', "\xff\xff\xff\xff");
unpack_row('Is', "\xff\xff\xff\xff");
unpack_row('Gs', "\x3f\xc0\x00\x00");
unpack_row('gs', "\x00\x00\xc0\x3f");
unpack_row('Es', "\x3f\xf8\x00\x00\x00\x00\x00\x00");
unpack_row('es', "\x00\x00\x00\x00\x00\x00\xf8\x3f");
var_dump(unpack('f', pack('f', 1.5)) === unpack('f', pack('f', 1.5)));
var_dump(unpack('ds', pack('d', 0.1))['s'] === 0.1);

echo "## the string codes and what each one keeps\n";
unpack_row('a5s', 'abcdefgh');
unpack_row('A*s', "ab \0 \t\r\n");
unpack_row('a*s', "ab \0 \t\r\n");
unpack_row('Z*s', "ab\0cd");
unpack_row('Z3s', "ab\0cd");
unpack_row('a0s', 'abc');
unpack_row('H*s', "\x12\x34");
unpack_row('h*s', "\x12\x34");
unpack_row('H3s', "\x12\x34");
unpack_row('H1s', "\x12\x34");
unpack_row('H0s', "\x12\x34");
unpack_row('h3s', "\x12\x34");

echo "## the cursor codes take no input of their own\n";
unpack_row('x2C', "ab\x05");
unpack_row('C0a', "\x05");
unpack_row('CXCs', "\x05\x06");
unpack_row('C@0Cs', "\x05\x06");
unpack_row('@2Cs', "\x05\x06\x07");

echo "## the offset is where reading starts\n";
unpack_row('Nx', 'abcdef', 2);
unpack_row('Cs', 'abc', 2);
unpack_row('C*', 'abc', 3);
unpack_row('a*s', 'abcdef', 4);
?>
--EXPECT--
## the three naming rules
N => {"1":16909060}
Nfoo => {"foo":16909060}
C* => {"1":97,"2":98,"3":99}
C*c => {"c1":97,"c2":98,"c3":99}
C3n => {"n1":97,"n2":98,"n3":99}
C1n => {"n":97}
Nfoo/Cbar => {"foo":16909060,"bar":9}
Ca/Cb => {"a":5,"b":6}
CX C => {"X C":5}
a*1 => {"1":"616263"}
C 2 => {" 2":5}
## the fixed-width codes read back what pack() wrote
cs => {"s":-1}
Cs => {"s":255}
ns => {"s":4660}
vs => {"s":13330}
Ns => {"s":287454020}
Vs => {"s":1144201745}
Js => {"s":-1}
Ps => {"s":1}
qs => {"s":-1}
Qs => {"s":-1}
ls => {"s":-1}
Ls => {"s":4294967295}
ss => {"s":-1}
Ss => {"s":65535}
is => {"s":-1}
Is => {"s":4294967295}
Gs => {"s":1.5}
gs => {"s":1.5}
Es => {"s":1.5}
es => {"s":1.5}
bool(true)
bool(true)
## the string codes and what each one keeps
a5s => {"s":"6162636465"}
A*s => {"s":"6162"}
a*s => {"s":"6162200020090d0a"}
Z*s => {"s":"6162"}
Z3s => {"s":"6162"}
a0s => {"s":""}
H*s => {"s":"31323334"}
h*s => {"s":"32313433"}
H3s => {"s":"313233"}
H1s => {"s":"31"}
H0s => {"s":""}
h3s => {"s":"323134"}
## the cursor codes take no input of their own
x2C => []
C0a => []
CXCs => {"XCs":5}
C@0Cs => {"@0Cs":5}
@2Cs => []
## the offset is where reading starts
Nx => {"x":1667523942}
Cs => {"s":99}
C* => []
a*s => {"s":"6566"}
