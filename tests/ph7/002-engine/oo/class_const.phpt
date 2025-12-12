--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Class constant compilation
--FILE--
<?php
class A { const C = 42; }
echo A::C . "\n";
?>
--EXPECT--
42
