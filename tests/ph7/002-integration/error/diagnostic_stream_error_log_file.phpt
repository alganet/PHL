--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A diagnostic's log copy goes to the error_log destination, not to stderr
--DESCRIPTION--
With log_errors on, the `error_log` directive decides where the LOG copy of a
runtime diagnostic lands: the error stream when it is unset, and the named file
-- timestamped, appended -- when it is not. A program that points its log at a
file before it does anything else is the ordinary case, and stderr must then be
clean, which is what --EXPECT_STDERR-- pins here.
--INI--
display_errors=0
log_errors=1
--FILE--
<?php
$log = sys_get_temp_dir() . "/phl_errlog_diag_" . getmypid() . ".log";
@unlink($log);
ini_set('error_log', $log);
echo "OUT\n";
$a = [];
$x = $a["k"];
echo "DONE\n";
$s = str_replace("\r\n", "\n", (string)@file_get_contents($log));
$s = preg_replace('/^\[\d\d-[A-Z][a-z]{2}-\d{4} \d\d:\d\d:\d\d [^\]]+\] /m', '<TS> ', $s);
// The location is this file's own scratch copy: keep the shape, drop the path.
echo preg_replace('/ in .* on line \d+$/m', ' in <FILE> on line <N>', $s);
@unlink($log);
?>
--EXPECT--
OUT
DONE
<TS> PHP Warning:  Undefined array key "k" in <FILE> on line <N>
--EXPECT_STDERR--
