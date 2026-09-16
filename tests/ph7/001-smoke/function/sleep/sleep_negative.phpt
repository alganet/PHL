--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sleep() rejects a negative delay with a ValueError
--FILE--
<?php
try { sleep(-1); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
// A zero delay is valid and returns int(0), not a bool.
var_dump(sleep(0));
?>
--EXPECT--
sleep(): Argument #1 ($seconds) must be greater than or equal to 0
int(0)
--CLEAN--
<?php
