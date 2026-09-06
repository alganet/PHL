--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr with empty string
--FILE--
<?php
var_dump(substr("", 0, 1));
?>
--EXPECTF--
%Astring(0) ""%A
--CLEAN--
<?php

