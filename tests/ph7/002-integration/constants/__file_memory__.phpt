--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
__FILE__ constant reports php's "Command line code" for inline (-r) code
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
__FILE__=Command line code
--CLEAN--
<?php
unset($phl, $fp, $out);
