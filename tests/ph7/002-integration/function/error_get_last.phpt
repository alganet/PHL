--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
error_get_last() reports the last unhandled diagnostic
--FILE--
<?php
error_reporting(0);
echo var_export(error_get_last() === null, true), "\n";
trigger_error("first problem", E_USER_WARNING);
$eglE = error_get_last();
echo "type=", $eglE['type'], "\n";
echo "message=", $eglE['message'], "\n";
echo "line=", $eglE['line'], "\n";
echo "file matches: ", var_export($eglE['file'] === __FILE__, true), "\n";
?>
--EXPECT--
true
type=512
message=first problem
line=4
file matches: true
--CLEAN--
<?php
