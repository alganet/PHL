--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Break with level exceeding available loops
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = '';
for ($i = 0; $i < 2; $i++) {
    $result .= "outer$i ";
    for ($j = 0; $j < 2; $j++) {
        if ($i == 1 && $j == 0) {
            break 3; // Invalid level
        }
        $result .= "inner$j ";
    }
}
$result .= "end";
echo $result;
?>
--EXPECTF--
%s %d Error:  A 'break' statement may only be used within a loop or switch
Compile error
--CLEAN--
<?php
unset($result);
