--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Direct named reference assignment (in function local scope)
--FILE--
<?php
function reflocalf(){
    $b = 5;
    $a =& $b; // local reference
    echo $a . "\n";
}
reflocalf();
echo isset($a) ? 'GLOBALS' : 'NO_GLOBAL';
?>
--EXPECT--
5
NO_GLOBAL
--CLEAN--
<?php
unset($b, $a);
