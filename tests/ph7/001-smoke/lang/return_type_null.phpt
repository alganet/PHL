--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
null standalone return type accepts an explicit null return (PHP 8.2)
--FILE--
<?php
function rtLitNullOk(): null { return null; }
echo rtLitNullOk() === null ? "null_ok\n" : "null_fail\n";
?>
--EXPECT--
null_ok
--CLEAN--
<?php
