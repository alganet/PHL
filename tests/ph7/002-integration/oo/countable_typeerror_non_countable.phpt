--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count() throws TypeError on object that does not implement Countable
--FILE--
<?php
class CountablePlain {}
count(new CountablePlain());
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: count(): Argument #1 ($value) must be of type Countable|array, %s given%A
--CLEAN--
<?php
