--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: no fallthrough — only the matching arm's result runs
--FILE--
<?php
$match_nf_calls = 0;
function match_nf_bump() {
    global $match_nf_calls;
    $match_nf_calls++;
    return 'x';
}
$r = match (1) {
    1 => match_nf_bump(),
    2 => match_nf_bump(),
    3 => match_nf_bump(),
};
echo $r, "\n";
echo $match_nf_calls, "\n";
?>
--EXPECT--
x
1
--CLEAN--
<?php
unset($calls, $r);
