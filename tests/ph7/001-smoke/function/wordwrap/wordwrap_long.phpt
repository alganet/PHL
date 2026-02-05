--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: wordwrap basic functionality
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo 'skip';
}
?>
--FILE--
<?php
// Test single word longer than width
$result6 = wordwrap("supercalifragilisticexpialidocious", 10);
echo strpos($result6, "\n") !== false ? "LONG_WORD_WRAP_OK\n" : "LONG_WORD_WRAP_FAIL\n";
?>
--EXPECT--
LONG_WORD_WRAP_OK
--CLEAN--
<?php
unset($result6);
