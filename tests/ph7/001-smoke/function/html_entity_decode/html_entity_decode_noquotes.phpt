--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
html_entity_decode with ENT_NOQUOTES flag
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test ENT_NOQUOTES flag - double quotes should not be decoded
$result = html_entity_decode('&quot;Hello&quot;', 4); // ENT_NOQUOTES
echo $result . "\n";
?>
--EXPECT--
&quot;Hello&quot;
--CLEAN--
<?php
unset($result);
