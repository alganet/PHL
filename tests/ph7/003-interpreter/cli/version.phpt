--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
phl interpreter CLI version
--SKIPIF--
<?php if (function_exists('zend_version')) { echo "skip"; } ?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$fp = popen("\"$phl\" --version", "r");
$out = '';
while (!feof($fp)) {
    $out .= fgets($fp);
}
fclose($fp);
echo $out;
?>
--EXPECTF--
PHL %d.%d.%d (cli) (built %s  %d %d %d:%d:%d)
Copyright (c) 2011-2014 Symisc Systems, 2025 Alexandre Gomes Gaigalas
