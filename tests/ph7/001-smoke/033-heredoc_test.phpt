--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Heredoc strings
--FILE--
<?php
$name = "World";
$message = <<<EOT
Hello $name!
This is a heredoc string.
EOT;
echo $message;
echo "\n";
$simple = <<<EOF
No variables here.
EOF;
echo $simple;
?>
--EXPECT--
Hello World!
This is a heredoc string.
No variables here.
--CLEAN--
<?php
unset($name, $message, $simple);
?>