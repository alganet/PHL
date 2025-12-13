--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: Label defined but not referenced should emit a warning
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
function foo() {
    label_unreferenced:
    echo "hello";
}
?>
--EXPECTF--
%s 3 Warning: Label 'label_unreferenced' is defined but not referenced
%s 3 Warning: Label 'label_unreferenced' is defined but not referenced
