--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Anonymous functions
--FILE--
<?php
$func = function($x) {
    return $x * 2;
};
echo $func(5); // 10
?>
--EXPECT--
10
--CLEAN--
<?php
unset($func);
?>