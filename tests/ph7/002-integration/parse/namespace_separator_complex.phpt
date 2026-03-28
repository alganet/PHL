--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Undefined namespace constant produces error
--FILE--
<?php
$test = \namespace\path\to\some\constant;
echo "ok\n";
?>
--EXPECTF--
%s Fatal error:  %s
--CLEAN--
<?php
