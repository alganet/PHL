--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: usable as function argument, array element, string concat, and return value
--FILE--
<?php
function match_as_expr_label(int $code): string {
    return match ($code) {
        0 => 'ok',
        1 => 'warn',
        default => 'err',
    };
}
echo match_as_expr_label(0), "\n";
echo "status=" . match (1) { 1 => 'open', 0 => 'closed' }, "\n";
$arr = [match (2) { 1 => 'a', 2 => 'b' }, 'c'];
echo $arr[0], $arr[1], "\n";
echo strtoupper(match (3) { 3 => 'hit' }), "\n";
?>
--EXPECT--
ok
status=open
bc
HIT
--CLEAN--
<?php
