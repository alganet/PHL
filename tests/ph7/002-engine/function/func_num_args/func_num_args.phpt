--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
func_num_args returns the number of args passed to the current function
--FILE--
<?php
function f1_num_args($a, $b, $c){
    echo func_num_args() . "\n";
}

f1_num_args(1,2,3);
?>
--EXPECT--
3
