--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr with invalid offset
--FILE--
<?php
var_dump(substr("abc", 10));
?>
--EXPECTF--
%Astring(0) ""%A
--CLEAN--
<?php

