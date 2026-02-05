--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
uniqid returns a non-empty string
--FILE--
<?php
$s = uniqid();
echo (is_string($s) && strlen($s) > 0) ? "ok\n" : "fail\n";
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($s);
