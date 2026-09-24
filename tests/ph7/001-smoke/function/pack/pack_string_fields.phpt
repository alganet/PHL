--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
pack() pads, truncates and NUL-terminates its five string fields
--FILE--
<?php
/* a/A/Z take ONE argument each and lay it into a field of exactly the width the
 * repeater asks for -- padded when the value is short, cut when it is long.
 * h/H take one argument too, but count NIBBLES rather than bytes. */
function pack_str(string $fmt, ...$values): void {
    echo $fmt, '=', bin2hex(pack($fmt, ...$values)), "\n";
}
echo "## the field is the width, whatever the value is\n";
pack_str('a5', 'ab');
pack_str('A5', 'ab');
pack_str('Z5', 'ab');
pack_str('a2', 'abcdef');
pack_str('A2', 'abcdef');
pack_str('Z2', 'abcdef');
pack_str('a0', 'ab');
pack_str('A0', 'ab');
pack_str('Z0', 'ab');
pack_str('Z1', 'ab');
pack_str('a', 'ab');
pack_str('A', '');

echo "## '*' is the value's own length; Z adds its terminator to it\n";
pack_str('a*', 'abcdef');
pack_str('A*', 'abcdef');
pack_str('Z*', 'abcdef');
pack_str('a*', '');
pack_str('Z*', '');

echo "## a NUL inside the value is a byte like any other\n";
pack_str('a4', "a\0b");
pack_str('Z4', "a\0b");
pack_str('a*', "a\0b");

echo "## two fields in a row each take their own argument\n";
pack_str('a2A2Z2', 'x', 'y', 'z');
pack_str('a*a*', 'ab', 'cd');

echo "## h fills the low nibble of a byte first, H the high one\n";
pack_str('H*', '1234abc');
pack_str('h*', '1234abc');
pack_str('H3', '1234abc');
pack_str('h3', 'abc');
pack_str('H5', 'abcde');
pack_str('h1', 'ab');
pack_str('H1', 'ab');
pack_str('H0', 'ab');
pack_str('h0', 'ab');
pack_str('H2H2', 'ff', '00');
pack_str('H*', 'ABCDEF');

echo "## a string field coerces its argument like any string parameter\n";
pack_str('a3', 123);
pack_str('a*', 12345);
pack_str('a*', true);
pack_str('a*', null);
pack_str('H*', 12);
?>
--EXPECT--
## the field is the width, whatever the value is
a5=6162000000
A5=6162202020
Z5=6162000000
a2=6162
A2=6162
Z2=6100
a0=
A0=
Z0=
Z1=00
a=61
A=20
## '*' is the value's own length; Z adds its terminator to it
a*=616263646566
A*=616263646566
Z*=61626364656600
a*=
Z*=00
## a NUL inside the value is a byte like any other
a4=61006200
Z4=61006200
a*=610062
## two fields in a row each take their own argument
a2A2Z2=780079207a00
a*a*=61626364
## h fills the low nibble of a byte first, H the high one
H*=1234abc0
h*=2143ba0c
H3=1230
h3=ba0c
H5=abcde0
h1=0a
H1=a0
H0=
h0=
H2H2=ff00
H*=abcdef
## a string field coerces its argument like any string parameter
a3=313233
a*=3132333435
a*=31
a*=
H*=12
