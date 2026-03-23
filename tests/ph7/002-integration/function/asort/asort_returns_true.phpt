--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
asort returns true
--FILE--
<?php
$a = array(3, 1, 2);
echo asort($a) ? "true" : "false";
?>
--EXPECT--
true
--CLEAN--
<?php
unset($a);
