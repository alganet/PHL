--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hexdec with long hex and invalid character behavior
--FILE--
<?php
echo "long:" . hexdec('0123456789abcdef0') . "\n";
echo "invalid:" . hexdec('DEADBEEFZ') . "\n";
?>
--EXPECTF--
%Along:1311768467463790320%AInvalid characters passed for attempted conversion, these have been ignored%Ainvalid:3735928559%A
--CLEAN--
<?php

