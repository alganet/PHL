--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: subject is a function call / arithmetic expression, evaluated once
--FILE--
<?php
$match_subject_calls = 0;
function match_subject_counter() {
    global $match_subject_calls;
    $match_subject_calls++;
    return 5;
}
$r = match (match_subject_counter() + 1) {
    5 => 'five',
    6 => 'six',
    7 => 'seven',
};
echo $r, "\n";
echo $match_subject_calls, "\n";
?>
--EXPECT--
six
1
--CLEAN--
<?php
unset($calls);
