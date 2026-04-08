--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Foreach by reference with early break retains last value
--FILE--
<?php
$arr = array(10, 20, 30);
foreach ($arr as &$v) {
    if ($v == 20) break;
}
echo "v=$v\n";
unset($v);
echo "done\n";
?>
--EXPECT--
v=20
done
--CLEAN--
<?php
