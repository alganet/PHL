--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
rsort should sort values in reverse and reindex keys
--FILE--
<?php
$a = array(1,3,2);
rsort($a);
foreach($a as $v) echo $v.PHP_EOL;
?>
--EXPECT--
3
2
1
--CLEAN--
<?php
unset($a);
