--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
explode with empty delimiter returns false
--FILE--
<?php
$result = explode("", "test");
echo $result === false ? "EXPLODE_EMPTY_DELIM:FALSE\n" : "EXPLODE_EMPTY_DELIM:TRUE\n";
?>
--EXPECT--
EXPLODE_EMPTY_DELIM:FALSE
--CLEAN--
<?php
unset($result);
