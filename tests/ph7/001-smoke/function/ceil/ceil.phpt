--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: ceil(2.1) returns 3
--FILE--
<?php
$val = ceil(2.1);
echo "ceil=" . (int)$val . "\n";
?>
--EXPECT--
ceil=3
--CLEAN--
<?php
unset($val);
