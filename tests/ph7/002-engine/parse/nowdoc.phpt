--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nowdoc strings
--FILE--
<?php
$name = "World";
$message = <<< 'EOT'
Hello $name!
This is a nowdoc string.
EOT;
echo $message;
echo "\n";
$simple = <<< 'EOF'
No variables here.
EOF;
echo $simple;
?>
--EXPECT--
Hello $name!
This is a nowdoc string.
No variables here.