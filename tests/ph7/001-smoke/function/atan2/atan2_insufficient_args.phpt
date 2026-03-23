--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan2 with negative values
--FILE--
<?php
$val = atan2(-1, -1);
echo "atan2=" . sprintf('%.8f', $val) . "\n";
?>
--EXPECT--
atan2=-2.35619449
--CLEAN--
<?php
unset($val);
