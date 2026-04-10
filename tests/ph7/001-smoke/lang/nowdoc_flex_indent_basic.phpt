--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 7.3 nowdoc with indented closing marker
--FILE--
<?php
$name = "ignored";
$x = <<<'EOT'
    $name is literal
    no interpolation
    EOT;
echo "[$x]\n";
echo strlen($x) . "\n";
--EXPECT--
[$name is literal
no interpolation]
33
--CLEAN--
<?php
unset($name, $x);
