--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A label nobody jumps to is silent (php has no "defined but not referenced" warning)
--FILE--
<?php
function foo() {
    label_unreferenced:
    echo "hello";
}
echo "no-diagnostic";
?>
--EXPECT--
no-diagnostic
--CLEAN--
<?php
