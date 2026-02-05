--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_bool with no arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = is_bool();
echo $result ? "true" : "false";
echo "\n";
?>
--EXPECT--
false
--CLEAN--
<?php
unset($result);
