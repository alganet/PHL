--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array syntax: empty array
--FILE--
<?php
$a = [];
echo count($a), "\n";
echo is_array($a) ? "yes" : "no", "\n";
?>
--EXPECT--
0
yes
--CLEAN--
<?php
