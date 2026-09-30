--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A case label's colon is the one outside its ternary
--DESCRIPTION--
Regression for GenStateCompileCaseExpr: the case expression was delimited at the
FIRST colon outside parentheses, so a ternary inside the label was cut at its own
`:` and the remainder came back as `syntax error, unexpected token ":"`. It is
how nette/utils spells the version-dependent arm of its token switch --
`case \PHP_VERSION_ID < 80100 ? \T_CLASS : \T_ENUM:` -- so no phpstan run got
past its own bootstrap. A `?:` closes itself (its two tokens are adjacent) and a
named argument's colon sits inside the call's parens, where the label scan never
looked.
--FILE--
<?php
function scttl_f($a = 1, $b = 2) { return $b; }

function scttl_pick($v) {
    switch ($v) {
        case PHP_INT_SIZE < 4 ? 'a' : 'b':
            return 'ternary';
        case 1 ? 2 : 3:
            return 'two';
        case scttl_f(b: 7):
            return 'named';
        case 0 ?: 9:
            return 'elvis';
        case (5 > 1 ? 11 : 12):
            return 'parenthesized';
        case true ? (false ? 20 : 21) : 22:
            return 'nested';
        default:
            return 'default';
    }
}
foreach (['b', 2, 7, 9, 11, 21, 'zz'] as $scttl_v) {
    echo $scttl_v, ' => ', scttl_pick($scttl_v), "\n";
}

// The alternative syntax delimits its own body with a colon too.
switch (2):
    case 1 ? 2 : 3:
        echo "alt-syntax reached\n";
        break;
endswitch;
?>
--EXPECT--
b => ternary
2 => two
7 => named
9 => elvis
11 => parenthesized
21 => nested
zz => default
alt-syntax reached
--CLEAN--
<?php
unset($scttl_v);
