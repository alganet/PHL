--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
htmlspecialchars_decode() coerces a scalar argument to string
--FILE--
<?php
echo htmlspecialchars_decode(123), "\n";
?>
--EXPECT--
123
--CLEAN--
<?php
