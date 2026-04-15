--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
throw expression: uncaught exception at script top level triggers a fatal
--FILE--
<?php
$value = null;
$result = $value ?? throw new Exception('required');
echo "never\n";
?>
--EXPECTF--
%AUncaught Exception: required%A
--CLEAN--
<?php
