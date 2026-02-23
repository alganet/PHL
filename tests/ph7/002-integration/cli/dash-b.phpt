--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
phl interpreter CLI bytecode dump
--SKIPIF--
<?php
// Only run under PHL; PHP has its own "phl" binary unrelated to this test.
if (!defined('PH7_VERSION')) {
    echo "skip";
}
?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$fp = popen("\"$phl\" -b \"./examples/hello_world.php\"", "r");
$out = '';
while (!feof($fp)) {
    $out .= fgets($fp);
}
fclose($fp);
echo $out;
?>
--EXPECT--
====================================================
PH7 VM Dump
====================================================
LOADC              0       99        0 [0]
CONSUME            1        0        0 [1]
LOADC              0      100        0 [2]
CONSUME            1        0        0 [3]
DONE               0        0        0 [4]
Hello World!
--CLEAN--
<?php
unset($phl, $fp, $out, $out .);
