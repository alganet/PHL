--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php_sapi_name() returns "cli" under the command-line interface
--FILE--
<?php
// Both PHL and the real php CLI report "cli" here.
echo php_sapi_name(), "\n";
?>
--EXPECT--
cli
--CLEAN--
<?php
?>
