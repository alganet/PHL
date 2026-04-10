--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 7.3 heredoc with indented closing marker
--FILE--
<?php
$x = <<<EOT
    hello
    world
    EOT;
echo "[$x]\n";
echo strlen($x) . "\n";
--EXPECT--
[hello
world]
11
--CLEAN--
<?php
unset($x);
