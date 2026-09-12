--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
phl reads a script from stdin when given no file (php parity); `--` passes the rest as script args
--SKIPIF--
<?php if (PHP_OS == 'WINNT') { echo "skip"; } ?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$tmp = tempnam(sys_get_temp_dir(), 'phl_stdin_');
// Feed a script on stdin; `--` ends options so alpha/beta become $argv[1..].
$fp = popen('"' . $phl . '" -- alpha beta > "' . $tmp . '"', 'w');
fwrite($fp, '<?php echo implode(",", $argv);');
pclose($fp);
echo file_get_contents($tmp), "\n";
unlink($tmp);
?>
--EXPECT--
Standard input code,alpha,beta
--CLEAN--
<?php
unset($phl, $fp, $tmp);
