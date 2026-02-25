--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ceil("2.1") should accept numeric strings and return 3
--FILE--
<?php
$val = ceil("2.1");
echo "ceil=" . (int)$val . "\n";
?>
--EXPECT--
ceil=3
--CLEAN--
<?php
unset($val);
