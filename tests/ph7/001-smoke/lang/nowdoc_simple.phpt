--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
nowdoc should not expand variables inside the document
--FILE--
<?php
$var = 'VALUE';
$s = <<<'EOT'
This is a nowdoc with $var not expanded.
EOT;
echo $s . "\n";
?>
--EXPECT--
This is a nowdoc with $var not expanded.
--CLEAN--
<?php
unset($var, $s);
