--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
phl interpreter CLI help
--SKIPIF--
<?php if (function_exists('zend_version')) { echo "skip"; } ?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$fp = popen("\"$phl\" --help", "r");
$out = '';
while (!feof($fp)) {
    $out .= fgets($fp);
}
fclose($fp);
echo $out;
?>
--EXPECT--
phl [-h|--help|-b|-v|--version|-r code] path/to/php_file [script args]
	-b: Dump PH7 byte-code instructions
	-r code: Run code from command line (no tags needed)
	-v, --version: Display version information and exit
	-h, --help: Display this message and exit
--CLEAN--
<?php
unset($phl, $fp, $out);
