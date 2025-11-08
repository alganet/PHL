--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Global variables
--FILE--
<?php
$global_var = "outside";
function test_global() {
    global $global_var;
    echo $global_var; // outside
    echo "\n";
    $global_var = "inside";
}
test_global();
echo $global_var; // inside
?>
--EXPECT--
outside
inside
--CLEAN--
<?php
unset($global_var);
?>