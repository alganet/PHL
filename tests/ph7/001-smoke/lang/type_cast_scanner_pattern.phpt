--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A type cast is one SCANNER pattern, aliases and blank rule included
--DESCRIPTION--
php matches the whole cast in its scanner --

    "(" [ \t]* NAME [ \t]* ")"

-- rather than letting the parser put `(`, a word and `)` back together, and two
of the things that follow from it were missing here. The NAME is matched as TEXT,
so php's two ALIASES are casts: `(double)` is `(float)` and `(binary)` is
`(string)`. Neither word is reserved anywhere else, which the second half checks:
`double` still names a function and a constant.

Only TABS AND SPACES may sit inside the parentheses, so a newline between them is
not a cast at all -- that half is pinned in
`002-integration/parse/cast_token_scanner_pattern.phpt`, where it is a parse
error, together with `(real)`, which php removed in the scanner and reports with
a sentence of its own.
--FILE--
<?php
$castVal = 1;
var_dump((double) $castVal, (binary) $castVal);
var_dump((DOUBLE) '2.5', (Binary) 7, (BINARY) true);
var_dump((double)$castVal, (	double	)$castVal);

/* The two aliases and the primary names answer identically. */
$castIn = '3.5';
var_dump((float) $castIn === (double) $castIn, (string) $castIn === (binary) $castIn);

/* Neither alias is a reserved word: both still name ordinary things. */
class CastAliasNames {
    const binary = 'not a cast';
    public function double($castX) { return $castX * 2; }
}
$castNames = new CastAliasNames();
var_dump($castNames->double(4), CastAliasNames::binary);
?>
--EXPECT--
float(1)
string(1) "1"
float(2.5)
string(1) "7"
string(1) "1"
float(1)
float(1)
bool(true)
bool(true)
int(8)
string(10) "not a cast"
