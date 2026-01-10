--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Empty nowdoc
--FILE--
<?php
$var = <<<'EOT'
EOT;
echo $var;
?>
--EXPECT--