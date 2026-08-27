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
phl [-h|--help|-b|-i|-l|-v|--version|-r code|--rf name|--rc name] path/to/php_file [script args]
phl -S host:port [-t docroot] [router.php]
	-b: Dump PH7 byte-code instructions
	-i: Display interpreter information and exit
	-l: Syntax-check (lint) the given file and exit
	-r code: Run code from command line (no tags needed)
	-S host:port: Start the built-in development server
	-t docroot: Document root for the server (default: current directory)
	-v, --version: Display version information and exit
	-h, --help: Display this message and exit
--CLEAN--
<?php
unset($phl, $fp, $out);
