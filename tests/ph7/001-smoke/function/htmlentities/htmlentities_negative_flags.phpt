--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
htmlentities with negative flags
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = htmlentities('"test"', -1);
echo $result . "\n";
?>
--EXPECT--
&quot;test&quot;
--CLEAN--
<?php
unset($result);
