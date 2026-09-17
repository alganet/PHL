--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
phl interpreter CLI version
--SKIPIF--
<?php
// Engine identity: this can never be cross-engine.
if (function_exists('zend_version')) { echo 'skip PHL-only by construction: asserts the --version banner (engine identity)'; }
?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$fp = popen("\"$phl\" --version", "r");
$out = fgets($fp);
fclose($fp);
echo $out;
?>
--EXPECTF--
PHL %d.%d.%d (cli)
--CLEAN--
<?php
unset($phl, $fp, $out);
