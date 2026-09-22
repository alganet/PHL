--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A simple "$a[offset]" subscript follows php's encaps_var_offset grammar, not the expression parser
--FILE--
<?php
// php parses a simple-syntax subscript with its OWN tiny grammar: a bare LABEL
// (always the STRING key, never a constant), an integer literal, '-' plus an
// integer literal, or a "$name" — and only a CANONICAL decimal is an INTEGER
// key. PHL fed the raw text to the expression compiler, so every other integer
// SPELLING silently answered a number.
set_error_handler(function ($no, $msg) { echo "Warning: $msg\n"; return true; });

$offsets = ['0', '1', '10', '007', '00', '0x1A', '0X1A', '0b11', '0o17', '017',
            '1_000', '0_1', '-1', '-0', '-007', 'x', '_x', 'x9', '_1', "\u{e9}",
            '9223372036854775807', '9223372036854775808', '1234567890123456789',
            '-9223372036854775808', '-9223372036854775809'];
$a = [];
foreach ($offsets as $off) {
    // Store under the key php's grammar says this offset means, then read it
    // back through the interpolation. Agreement means the compiler and the
    // hashmap read the same key.
    eval('$a = []; $a[' . var_export($off, true) . '] = "HIT"; $out = "$a[' . $off . ']";');
    echo str_pad($off, 22), $out === 'HIT' ? 'HIT' : 'MISS', "\n";
}

// A bare word is the string key, never a constant — even next to a real constant.
define('OFFSET_GRAMMAR_C', 'other');
$c = ['OFFSET_GRAMMAR_C' => 'word'];
echo "const: $c[OFFSET_GRAMMAR_C]\n";

// "$name" is the one offset php evaluates.
$k = 'x';
$v = ['x' => 'byvar'];
echo "var: $v[$k]\n";

// Everything else is a php PARSE ERROR — both engines throw a catchable
// ParseError out of eval() (the message wording differs).
$bad = ['$a[ 0]', '$a[0 ]', "\$a['x']", '$a[+1]', '$a[-$k]', '$a[[]', '$a[]',
        '$a[1.5]', '$a[1e2]', '$a[9x]', '$a[x-y]', '$a[0x]', '$a[1_]', '$a[1__0]',
        '$a[- 1]', '$a[$]', '$a[$k9x-]'];
foreach ($bad as $src) {
    try {
        eval('$a = []; $k = 0; $z = "' . $src . '";');
        echo "ACCEPTED: $src\n";
    } catch (ParseError $e) {
        echo "ParseError: $src\n";
    }
}
restore_error_handler();
?>
--EXPECT--
0                     HIT
1                     HIT
10                    HIT
007                   HIT
00                    HIT
0x1A                  HIT
0X1A                  HIT
0b11                  HIT
0o17                  HIT
017                   HIT
1_000                 HIT
0_1                   HIT
-1                    HIT
-0                    HIT
-007                  HIT
x                     HIT
_x                    HIT
x9                    HIT
_1                    HIT
é                    HIT
9223372036854775807   HIT
9223372036854775808   HIT
1234567890123456789   HIT
-9223372036854775808  HIT
-9223372036854775809  HIT
const: word
var: byvar
ParseError: $a[ 0]
ParseError: $a[0 ]
ParseError: $a['x']
ParseError: $a[+1]
ParseError: $a[-$k]
ParseError: $a[[]
ParseError: $a[]
ParseError: $a[1.5]
ParseError: $a[1e2]
ParseError: $a[9x]
ParseError: $a[x-y]
ParseError: $a[0x]
ParseError: $a[1_]
ParseError: $a[1__0]
ParseError: $a[- 1]
ParseError: $a[$]
ParseError: $a[$k9x-]
--CLEAN--
<?php
unset($offsets, $off, $a, $out, $c, $k, $v, $bad, $src, $e, $z);
