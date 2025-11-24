--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
urldecode decodes a URL-encoded string
--FILE--
<?php
echo urldecode('a+b') . "\n";
?>
--EXPECT--
a b
