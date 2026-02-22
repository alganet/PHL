--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: abs(0) returns 0
--FILE--
<?php
echo "abs=" . abs(0) . "\n";
?>
--EXPECT--
abs=0
--CLEAN--
<?php

?>

