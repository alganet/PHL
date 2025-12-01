--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_a returns true for instance of class
--FILE--
<?php
class IsA_Test_2025 {}
$o = new IsA_Test_2025();
echo (is_a($o, 'IsA_Test_2025') ? "ok\n" : "fail\n");
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($o);
?>
