--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Symmetric array destructuring with invalid expression is fatal error
--FILE--
<?php
[1] = array(1);
echo "should not reach here";
?>
--EXPECTF--
PHP Fatal error:  %s
--CLEAN--
<?php
