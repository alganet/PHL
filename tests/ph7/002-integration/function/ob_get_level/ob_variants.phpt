--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Output buffering APIs: ob_start, ob_get_level, ob_get_contents, ob_get_clean
--SKIPIF--
<?php
if (function_exists('zend_version')) { echo "skip: not PH7\n"; }
?>
--FILE--
<?php
echo ob_get_level() . "\n";
ob_start();
echo "1\n";
echo ob_get_level() . "\n";
echo ob_get_contents() . "\n";
// Use get_clean to capture and end
$s = ob_get_clean();
echo "caught=" . (strpos($s,'1') !== false ? 'yes' : 'no') . "\n";
// Start new buffer and discard
ob_start();
echo "2";
ob_end_clean();
echo ob_get_level() . "\n";
?>
--EXPECT--
0
caught=yes
0
--CLEAN--
<?php
unset($s);
