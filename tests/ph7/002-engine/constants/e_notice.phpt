--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: E_NOTICE constant
--FILE--
<?php
echo "E_NOTICE=" . E_NOTICE . "\n";
?>
--EXPECTF--
E_NOTICE=%d
