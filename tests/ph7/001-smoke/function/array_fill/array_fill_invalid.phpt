--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: array_fill with negative num returns false
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
$result = array_fill(0, -1, 'test');
echo gettype($result) . ':';
if (is_array($result)) {
    echo count($result);
} elseif (is_string($result)) {
    echo strlen($result);
} else {
    echo $result;
}
echo "\n";
?>
--EXPECT--
array:1
--CLEAN--
<?php
unset($result);
