--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: __DIR__ for inline (-r) code is "." (php resolves the cwd absolute path — recorded divergence)
--SKIPIF--
<?php if (function_exists('zend_version')) { echo "skip php resolves __DIR__ of Command line code to the absolute cwd"; } ?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$fp = popen("\"$phl\" -r \"echo \\\"__DIR__=\\\" . __DIR__ . \\\"\\n\\\";\"", "r");
$out = '';
while (!feof($fp)) {
    $out .= fgets($fp);
}
fclose($fp);
echo $out;
?>
--EXPECT--
__DIR__=.
--CLEAN--
<?php
unset($phl, $fp, $out);
