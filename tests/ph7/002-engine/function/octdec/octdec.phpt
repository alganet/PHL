--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
octdec returns decimal value of octal string
--FILE--
<?php
echo octdec('10') . "\n"; // 8
?>
--EXPECT--
8
