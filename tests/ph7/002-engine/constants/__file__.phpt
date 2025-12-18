--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: __FILE__ constant returns script path
--FILE--
<?php
echo "__FILE__=" . __FILE__ . "\n";
?>
--EXPECTF--
__FILE__=%s