--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
round with negative number rounds correctly
--FILE--
<?php
$val = round(-3.5);
echo "round=" . (int)$val . "\n";
?>
--EXPECT--
round=-4
--CLEAN--
<?php
unset($val);
