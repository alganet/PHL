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
$script = tempnam(sys_get_temp_dir(), 'phlb');
file_put_contents($script, "<?php\n\necho \"Hello World!\";");
$fp = popen("\"$phl\" -b \"$script\"", "r");
$out = '';
while (!feof($fp)) {
    $out .= fgets($fp);
}
fclose($fp);
@unlink($script);
echo $out;
?>
--EXPECTF--
====================================================
PH7 VM Dump
====================================================
NSSWITCH           0        0        0 [0] L%d
LOADC              0 %A [1] L%d
CONSUME            1        0        0 [2] L%d
LOADC              0 %A [3] L%d
CONSUME            1        0        0 [4] L%d
DONE               0        0        0 [5] L%d
Hello World!
--CLEAN--
<?php
unset($phl, $script, $fp, $out);
