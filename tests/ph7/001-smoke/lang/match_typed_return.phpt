--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: used as the return value of a typed function
--FILE--
<?php
function match_typed_classify(int $n): string {
    return match (true) {
        $n < 0   => 'negative',
        $n === 0 => 'zero',
        default  => 'positive',
    };
}
echo match_typed_classify(-3), "\n";
echo match_typed_classify(0), "\n";
echo match_typed_classify(5), "\n";
?>
--EXPECT--
negative
zero
positive
--CLEAN--
<?php
