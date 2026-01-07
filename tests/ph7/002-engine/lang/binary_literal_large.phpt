--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Large binary literal
--FILE--
<?php
echo 0b11111111111111111111111111111111;
?>
--EXPECT--
4294967295