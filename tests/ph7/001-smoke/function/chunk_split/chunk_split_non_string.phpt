--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chunk_split() coerces a scalar argument to string
--FILE--
<?php
echo rtrim(chunk_split(123)), "\n";
?>
--EXPECT--
123
--CLEAN--
<?php
