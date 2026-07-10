--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Appending to $GLOBALS is a fatal error (PHP 8.1)
--FILE--
<?php
$GLOBALS[] = 1;
?>
--EXPECTF--
%ACannot append to $GLOBALS%A
--CLEAN--
<?php
