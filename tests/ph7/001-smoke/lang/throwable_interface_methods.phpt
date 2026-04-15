--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Throwable: both Exception and Error implement all interface methods
--FILE--
<?php
$e = new Exception("msg-e", 42);
echo $e->getMessage(), "\n";
echo $e->getCode(), "\n";
echo ($e->getPrevious() === null) ? "no-prev\n" : "has-prev\n";

$r = new Error("msg-r", 7);
echo $r->getMessage(), "\n";
echo $r->getCode(), "\n";
echo ($r->getPrevious() === null) ? "no-prev\n" : "has-prev\n";
echo is_int($r->getLine()) ? "line-int\n" : "line-other\n";
echo is_array($r->getTrace()) ? "trace-array\n" : "trace-other\n";
?>
--EXPECT--
msg-e
42
no-prev
msg-r
7
no-prev
line-int
trace-array
--CLEAN--
<?php
