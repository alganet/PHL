--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: continue statement outside loop
--FILE--
<?php
continue;
?>
--EXPECTF--
%AFatal error:%A'continue' not in the 'loop' or 'switch' context%A
--CLEAN--
<?php

