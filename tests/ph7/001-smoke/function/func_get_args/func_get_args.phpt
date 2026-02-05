--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
func_get_args returns the list of arguments as an array
--FILE--
<?php
function f1_get_args($a, $b){
    $args = func_get_args();
    echo count($args) . "\n";
    echo $args[1] . "\n";
}

f1_get_args('one', 'two');
?>
--EXPECT--
2
two
--CLEAN--
<?php
unset($args);
