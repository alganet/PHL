--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto with complex label references
--FILE--
<?php
function test_goto() {
    $x = 1;
    goto label_a;
    label_b:
    echo "b ";
    goto label_c;
    label_a:
    echo "a ";
    if ($x < 3) {
        $x++;
        goto label_b;
    }
    label_c:
    echo "c ";
}
test_goto();
?>
--EXPECT--
a b c
--CLEAN--
<?php
unset($x);
