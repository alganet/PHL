--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
phl interpreter CLI -l lints a file (syntax-check only), matching PHP
--SKIPIF--
<?php if (PHP_OS == 'WINNT') { echo "skip"; } ?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$dir = sys_get_temp_dir();
$ok  = $dir . '/phl_lint_ok_' . getmypid() . '.php';
$bad = $dir . '/phl_lint_bad_' . getmypid() . '.php';
file_put_contents($ok,  "<?php echo 1;\n");
file_put_contents($bad, "<?php function (\n");

// valid file -> success line, exit 0
$fp = popen("\"$phl\" -l \"$ok\" 2>&1", 'r');
$o = ''; while (!feof($fp)) { $o .= fgets($fp); } $rc = pclose($fp);
echo $o;
echo "ok_exit=" . ($rc === 0 ? 0 : 1) . "\n";

// broken file -> error summary, nonzero exit
$fp = popen("\"$phl\" -l \"$bad\" 2>&1", 'r');
$o = ''; while (!feof($fp)) { $o .= fgets($fp); } $rc = pclose($fp);
echo (strpos($o, 'Errors parsing') !== false) ? "bad_reports_error\n" : "bad_no_error\n";
echo "bad_exit=" . ($rc === 0 ? 0 : 1) . "\n";
?>
--EXPECTF--
No syntax errors detected in %s
ok_exit=0
bad_reports_error
bad_exit=1
--CLEAN--
<?php
@unlink(sys_get_temp_dir() . '/phl_lint_ok_' . getmypid() . '.php');
@unlink(sys_get_temp_dir() . '/phl_lint_bad_' . getmypid() . '.php');
