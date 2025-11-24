--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
urlencode encodes a string for URLs
--FILE--
<?php
echo urlencode('a b') . "\n";
?>
--EXPECT--
a+b
