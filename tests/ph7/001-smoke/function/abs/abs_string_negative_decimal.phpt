--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: abs("-2.5") should return 2.5 (negative numeric string)
--FILE--
<?php
echo "abs=" . abs("-2.5") . "\n";
?>
--EXPECT--
abs=2.5
--CLEAN--
<?php

?>

