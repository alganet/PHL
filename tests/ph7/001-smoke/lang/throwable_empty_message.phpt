--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Throwable: default message is empty string
--FILE--
<?php
$e = new Exception();
echo "Exception:[", $e->getMessage(), "]\n";
echo strlen($e->getMessage()), "\n";
$r = new Error();
echo "Error:[", $r->getMessage(), "]\n";
echo strlen($r->getMessage()), "\n";
?>
--EXPECT--
Exception:[]
0
Error:[]
0
--CLEAN--
<?php
