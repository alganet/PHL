--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Re-assigning $GLOBALS is a fatal error (PHP 8.1)
--FILE--
<?php
$GLOBALS = [1];
?>
--EXPECTF--
%A$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax%A
--CLEAN--
<?php
