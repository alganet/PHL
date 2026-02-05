--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test nowdoc with trailing spaces in delimiter
--FILE--
<?php
$nowdoc = <<<'EOT'
This is a nowdoc
EOT;
echo $nowdoc;
?>
--EXPECT--
This is a nowdoc
--CLEAN--
<?php
unset($nowdoc);
