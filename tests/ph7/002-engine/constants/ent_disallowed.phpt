--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: ENT_DISALLOWED constant
--FILE--
<?php
echo "ENT_DISALLOWED=" . ENT_DISALLOWED . "\n";
?>
--EXPECTF--
ENT_DISALLOWED=%d
