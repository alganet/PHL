--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unset($GLOBALS) is a fatal error (PHP 8.1)
--FILE--
<?php
unset($GLOBALS);
?>
--EXPECTF--
%A$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax%A
--CLEAN--
<?php
