--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_declared_interfaces returns declared interfaces
--FILE--
<?php
interface ITest {}
$list = get_declared_interfaces();
if (is_array($list) && in_array('ITest', $list, true)) echo "OK\n"; else echo "FAIL\n";
?>
--EXPECT--
OK

--CLEAN--
<?php
unset($list);
?>
