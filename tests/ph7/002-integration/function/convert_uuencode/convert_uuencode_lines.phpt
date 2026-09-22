--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
convert_uuencode/convert_uudecode: line structure, binary data and invalid input
--DESCRIPTION--
Covers the parts of php's uuencode that only show up beyond one short line:
the 45-byte line split (a short LAST line carries its own length byte and is not
newline-terminated by the line loop), every byte value round-tripping, and the
three ways php refuses to decode -- an empty document, a length byte claiming more
than the input holds, and a truncated line -- each a warning plus false.
--FILE--
<?php
// Line structure around the 45-byte boundary.
foreach ([0, 1, 2, 3, 44, 45, 46, 89, 90, 91] as $n) {
    $enc = convert_uuencode(str_repeat('x', $n));
    printf("%2d len=%3d lines=%d rt=%s\n", $n, strlen($enc), substr_count($enc, "\n"),
        convert_uudecode($enc) === str_repeat('x', $n) ? 'ok' : 'BAD');
}

// Every byte value survives a round trip.
$all = '';
for ($i = 0; $i < 256; $i++) {
    $all .= chr($i);
}
$enc = convert_uuencode($all);
echo 'binary rt=', convert_uudecode($enc) === $all ? 'ok' : 'BAD', "\n";
// (hex, not the raw text: uuencoded data contains '%', which EXPECTF would read.)
echo 'first16=', bin2hex(substr($enc, 0, 16)), ' whole=', md5($enc), "\n";

// Invalid input: warning + false. '' is empty, 'M' claims 45 bytes it does not
// carry, "\n" claims 42, and '#86)' is one character short of its three-byte group.
foreach (['', 'M', "\n", '#86)'] as $bad) {
    $r = @convert_uudecode($bad);
    printf("%-8s => %s\n", json_encode($bad), var_export($r, true));
}
// A line whose length byte is honoured over the characters that follow it.
echo 'partial=', bin2hex(convert_uudecode('A' . str_repeat('A', 59))), "\n";
convert_uudecode('#86)');
?>
--EXPECTF--
 0 len=  2 lines=1 rt=ok
 1 len=  8 lines=2 rt=ok
 2 len=  8 lines=2 rt=ok
 3 len=  8 lines=2 rt=ok
44 len= 64 lines=2 rt=ok
45 len= 64 lines=2 rt=ok
46 len= 70 lines=3 rt=ok
89 len=126 lines=3 rt=ok
90 len=126 lines=3 rt=ok
91 len=132 lines=4 rt=ok
binary rt=ok
first16=4d606024226050302521403c28223048 whole=0168144bd79acb93691292f7907ce84a
""       => false
"M"      => false
"\n"     => false
"#86)"   => false
partial=861861861861861861861861861861861861861861861861861861861861861861
PHP Warning:  convert_uudecode(): Argument #1 ($data) is not a valid uuencoded string in %s on line %d
--CLEAN--
<?php
