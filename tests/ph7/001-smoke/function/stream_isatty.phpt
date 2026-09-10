--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
stream_isatty() on redirected/file streams is false
--FILE--
<?php
// Under the test harness stdout/stderr are redirected (not a terminal), and a
// regular file is never a terminal, so all of these are false on both engines.
var_dump(stream_isatty(STDOUT));
var_dump(stream_isatty(STDERR));
$f = fopen(tempnam(sys_get_temp_dir(), 'sia'), 'w');
var_dump(stream_isatty($f));
fclose($f);
?>
--EXPECT--
bool(false)
bool(false)
bool(false)
--CLEAN--
<?php
