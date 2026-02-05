--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
func_get_arg retrieves a single argument by index
--FILE--
<?php
function f1_get_arg($a, $b, $c){
    echo func_get_arg(0) . "\n";
    echo func_get_arg(2) . "\n";
}

f1_get_arg('first','second','third');
?>
--EXPECT--
first
third
--CLEAN--
<?php

