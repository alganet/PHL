--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Basic class
--FILE--
<?php
class TestClass {
    var $value = 42;
}

echo "42";
?>
--EXPECT--
42