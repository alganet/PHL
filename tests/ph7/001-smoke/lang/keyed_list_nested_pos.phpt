--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Keyed list destructuring: positional nested in keyed
--FILE--
<?php
["pt" => [$x, $y]] = ["pt" => [3, 4]];
echo "$x $y\n";
?>
--EXPECT--
3 4
--CLEAN--
<?php
unset($x, $y);
