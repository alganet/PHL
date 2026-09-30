--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A 0-byte PHP file is an empty program, not an IO error
--DESCRIPTION--
php compiles a 0-byte file to a program that does nothing: `-l` reports no syntax
errors, running it prints nothing and exits 0, and including it is a silent no-op
returning 1. PHL read every one of those as a failure -- `mmap()` refuses a zero
length and the whole-file stream read answered -1 for an empty blob -- so `phl
empty.php` said "Could not open input file" and `include` warned "IO error while
importing". Real trees carry such files (slevomat's emptyFile.php fixture is one).
--FILE--
<?php
$phl   = getenv('PHPT_TARGET_EXECUTABLE');
$empty = sys_get_temp_dir() . '/phl_emptyfile_' . getmypid() . '.php';
file_put_contents($empty, '');
var_dump(filesize($empty));

$fp = popen("\"$phl\" -l \"$empty\"", 'r');
$o = ''; while (!feof($fp)) { $o .= fgets($fp); } $rc = pclose($fp);
echo $o;
echo "lint_exit=$rc\n";

$fp = popen("\"$phl\" \"$empty\"", 'r');
$o = ''; while (!feof($fp)) { $o .= fgets($fp); } $rc = pclose($fp);
var_dump($o);
echo "run_exit=$rc\n";

var_dump(include $empty);
var_dump(require $empty);
var_dump(file_get_contents($empty));
@unlink($empty);
?>
--EXPECTF--
int(0)
No syntax errors detected in %s
lint_exit=0
string(0) ""
run_exit=0
int(1)
int(1)
string(0) ""
