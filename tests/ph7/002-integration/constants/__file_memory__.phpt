--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: __FILE__ constant returns :MEMORY: for inline code
--SKIPIF--
<?php if (function_exists('zend_version')) { echo "skip"; } ?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$fp = popen("\"$phl\" -r \"echo \\\"__FILE__=\\\" . __FILE__ . \\\"\\n\\\";\"", "r");
$out = '';
while (!feof($fp)) {
    $out .= fgets($fp);
}
fclose($fp);
echo $out;
?>
--EXPECT--
__FILE__=:MEMORY:
--CLEAN--
<?php
unset($phl, $fp, $out, $out .);
