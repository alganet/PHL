--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Heredoc string interpolation with curly braces
--FILE--
<?php
$name = 'world';
$array = array('key' => 'value');
echo <<<EOT
Hello {$name}!
The value is {$array['key']}.
EOT;
?>
--EXPECT--
Hello world!
The value is value.
--CLEAN--
<?php
unset($name, $array);
