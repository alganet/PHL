--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
parsing nested expressions with parenthesis
--FILE--
<?php
$result = ((1 + 2) * (3 + 4)) / ((5 - 1) + (6 * 2));
echo $result . "\n";
?>
--EXPECT--
1.3125
--CLEAN--
<?php
unset($result);
