--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Two functions differing only in case are one redeclaration
--FILE--
<?php
function frciTwice() { return 1; }
function FRCITWICE() { return 2; }
--EXPECTF--
PHP Fatal error:  Cannot redeclare function FRCITWICE() (previously declared in %s:2) in %s on line 3%A
--CLEAN--
<?php
