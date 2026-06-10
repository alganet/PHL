--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP_BINARY is defined and non-empty
--FILE--
<?php
echo (defined('PHP_BINARY') && PHP_BINARY !== '') ? "defined" : "missing", "\n";
?>
--EXPECT--
defined
--CLEAN--
<?php
?>
