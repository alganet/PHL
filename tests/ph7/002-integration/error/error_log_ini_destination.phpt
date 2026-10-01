--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
error_log() routes types 0 and unrecognised to the error_log ini destination
--DESCRIPTION--
The `error_log` directive names where a log line goes, and error_log()'s
message_type picks which logger is asked. Type 0 -- and every value php does
not recognise -- means "the configured logger", so with a destination set the
message is APPENDED there behind php's `[d-M-Y H:i:s e] ` timestamp; type 3
names its own file and is written verbatim, with neither timestamp nor newline;
type 4 is the SAPI logger by definition and reaches the error stream even when
a destination is set. Clearing the directive hands the rest of the run back to
that stream, and a destination that cannot be opened falls back to it silently
-- php answers TRUE for the fallback, because only type 3 reports a failure.
--INI--
display_errors=0
log_errors=1
--FILE--
<?php
$dir = sys_get_temp_dir();
$log = $dir . "/phl_errlog_dest_" . getmypid() . ".log";
$own = $dir . "/phl_errlog_own_" . getmypid() . ".log";
@unlink($log); @unlink($own);

function shown(string $f): string {
    $s = (string)@file_get_contents($f);
    // The timestamp is the wall clock: keep its SHAPE, drop its value.
    $s = preg_replace('/^\[\d\d-[A-Z][a-z]{2}-\d{4} \d\d:\d\d:\d\d [^\]]+\] /m', '<TS> ', $s);
    return str_replace("\r\n", "\n", $s);
}

ini_set('error_log', $log);
var_dump(ini_get('error_log') === $log);

// 0 and every unrecognised type go to the configured destination, timestamped.
var_dump(error_log("type zero", 0));
var_dump(error_log("type nine", 9));
// 3 names its own file and is written verbatim: no timestamp, no newline.
var_dump(error_log("A", 3, $own));
var_dump(error_log("B", 3, $own));
// 4 is the SAPI logger and never the destination.
var_dump(error_log("type four", 4));

echo "DEST:\n", shown($log);
echo "OWN:[", shown($own), "]\n";

// Clearing it hands the rest of the run back to the stream.
ini_set('error_log', '');
var_dump(error_log("cleared", 0));

// A destination that will not open falls back to the stream, and still TRUE.
ini_set('error_log', $dir . "/phl-no-such-dir/x.log");
var_dump(error_log("unopenable", 0));

@unlink($log); @unlink($own);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
DEST:
<TS> type zero
<TS> type nine
OWN:[AB]
bool(true)
bool(true)
--EXPECT_STDERR--
type four
cleared
unopenable
