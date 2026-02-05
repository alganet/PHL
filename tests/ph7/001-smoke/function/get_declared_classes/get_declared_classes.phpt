--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_declared_classes returns array and contains stdClass
--FILE--
<?php
$classes = get_declared_classes();
if (is_array($classes) && in_array('stdClass', $classes)) {
    echo "get_declared_classes_ok\n";
} else {
    echo "get_declared_classes_fail\n";
}
?>
--EXPECT--
get_declared_classes_ok
--CLEAN--
<?php
unset($classes);
