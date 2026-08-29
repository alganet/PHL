--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: stristr with before_needle parameter

--FILE--
<?php
$result = stristr("HELLO world", "wo", true);
echo $result . "\n";
?>
--EXPECT--
HELLO
--CLEAN--
<?php
unset($result);
