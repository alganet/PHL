--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Empty nowdoc strings
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$empty = <<<'EOD'
EOD;
echo "Empty nowdoc: '$empty'\n";
echo "Length: " . strlen($empty) . "\n";
echo "Done\n";
?>
--EXPECT--
--CLEAN--
<?php
unset($empty);
