--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ceil(-2.1) should round up to -2
--FILE--
<?php
$val = ceil(-2.1);
echo "ceil=" . (int)$val . "\n";
?>
--EXPECT--
ceil=-2
--CLEAN--
<?php
unset($val);
