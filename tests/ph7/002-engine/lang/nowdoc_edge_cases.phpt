--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nowdoc edge cases
--FILE--
<?php
// Test nowdoc with different delimiters and content
$nowdoc1 = <<< 'EOD'
This is a nowdoc string
with multiple lines
and no variable interpolation.
EOD;

$nowdoc2 = <<< 'EOF'
Another nowdoc
EOF;

echo $nowdoc1 . "\n";
echo $nowdoc2 . "\n";
?>
--EXPECT--
This is a nowdoc string
with multiple lines
and no variable interpolation.
Another nowdoc
--CLEAN--
<?php
unset($nowdoc1, $nowdoc2);
?>
