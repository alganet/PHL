--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: htmlspecialchars with ENT_NOQUOTES leaves quotes unescaped
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
$result = htmlspecialchars('"test" <>&', ENT_NOQUOTES);
echo $result . "\n";
?>
--EXPECT--
"test" &lt;&gt;&amp;
--CLEAN--
<?php
unset($result);
