--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ob_end_flush return value basic behavior
--FILE--
<?php
ob_start();
echo "ABC";
$r = ob_end_flush();
echo ($r ? "ok" : "fail");
?>
--EXPECT--
ABCok
--CLEAN--
<?php
unset($r);
