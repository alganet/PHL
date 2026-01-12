--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
urlencode encodes unsafe characters to hex
--FILE--
<?php
echo urlencode('a%b') . "\n";
?>
--EXPECT--
a%25b