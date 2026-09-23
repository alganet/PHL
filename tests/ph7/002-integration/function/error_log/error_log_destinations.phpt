--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
error_log() routes its message: type 3 appends the file, everything else logs
--FILE--
<?php
/* error_log() used to invoke the embedder callback and nothing else, so on the
 * CLI -- where there is no callback -- it was a total no-op that answered TRUE:
 * the message went nowhere, `error_log($m, 3, $f)` wrote no file, and a
 * destination that could not be opened still reported success. */
$log = tempnam(sys_get_temp_dir(), 'phl_errlog_');
@unlink($log);

var_dump(error_log("first\n", 3, $log));
var_dump(error_log("second\n", 3, $log));
var_dump(file_get_contents($log));   /* verbatim: no newline added, no timestamp */
@unlink($log);

/* An unopenable destination is php's open warning and FALSE. */
$bad = $log . DIRECTORY_SEPARATOR . 'no' . DIRECTORY_SEPARATOR . 'such';
var_dump(@error_log("nope\n", 3, $bad));

/* php removed the TCP/IP destination; the type is refused outright. */
try {
	error_log('x', 2, 'localhost');
} catch (ValueError $e) {
	echo get_class($e), ': ', $e->getMessage(), "\n";
}

/* Type 1 is mail(), which this engine has no transport for, so it answers FALSE
 * rather than claiming a delivery. It is deliberately NOT asserted here: php's
 * own answer depends on whether the host has an MTA (it shells out to sendmail
 * and prints its failure), so the case is not cross-engine assertable. */
?>
--EXPECT--
bool(true)
bool(true)
string(13) "first
second
"
bool(false)
ValueError: TCP/IP option is not available for error logging
