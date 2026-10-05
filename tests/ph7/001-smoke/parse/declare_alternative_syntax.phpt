--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`declare(...): ... enddeclare;` runs its body like the braced form, and its end keyword wants a `;`
--FILE--
<?php
// php's grammar takes `declare (...) ':' inner_statement_list T_ENDDECLARE ';'`
// beside the braced and the `;` forms; the whole construct is ONE statement,
// so it can be an unbraced if's body. A closing tag counts as the `;`.
declare(ticks=1):
    echo "a\n";
    if (1): echo "b\n"; endif;
    declare(ticks=2): echo "c\n"; enddeclare;
ENDDECLARE;
echo "d\n";

function dalt_f() { declare(ticks=1): return 5; enddeclare; }
echo dalt_f(), "\n";
if (1) declare(ticks=1): echo "e\n"; enddeclare; else echo "never\n";
eval('declare(ticks=1): ?>f<?php enddeclare; echo "\n";');
eval('declare(ticks=1): echo "g\n"; enddeclare ?>');

$cases = [
    'declare(ticks=1): echo 1;',
    'declare(ticks=1):',
    'declare(ticks=1): echo ""; enddeclare',
    'declare(ticks=1): echo ""; enddeclare echo 2;',
    "declare(ticks=1):\nenddeclare\n\n",
];
foreach ($cases as $code) {
    try {
        eval($code);
        echo "compiled: ", json_encode($code), "\n";
    } catch (ParseError $e) {
        echo $e->getMessage(), ' (line ', $e->getLine(), ') -- ', json_encode($code), "\n";
    }
}
--EXPECT--
a
b
c
d
5
e
f
g
syntax error, unexpected end of file (line 1) -- "declare(ticks=1): echo 1;"
syntax error, unexpected end of file (line 1) -- "declare(ticks=1):"
syntax error, unexpected end of file, expecting ";" (line 1) -- "declare(ticks=1): echo \"\"; enddeclare"
syntax error, unexpected token "echo", expecting ";" (line 1) -- "declare(ticks=1): echo \"\"; enddeclare echo 2;"
syntax error, unexpected end of file, expecting ";" (line 4) -- "declare(ticks=1):\nenddeclare\n\n"
