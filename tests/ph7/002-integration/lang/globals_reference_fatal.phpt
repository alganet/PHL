--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Acquiring a reference to $GLOBALS is a fatal error (PHP 8.1)
--FILE--
<?php
$r = &$GLOBALS;
?>
--EXPECTF--
%ACannot acquire reference to $GLOBALS%A
--CLEAN--
<?php
