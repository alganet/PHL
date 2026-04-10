--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 7.3 heredoc mixing tabs and spaces in the indent prefix is a parse error
--FILE--
<?php
$x = <<<EOT
	tab-prefixed body
    EOT;
--EXPECTF--
PHP %s error:  Invalid indentation - tabs and spaces cannot be mixed in %s on line %d
--CLEAN--
<?php
