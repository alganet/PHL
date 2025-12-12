--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ob_get_contents and ob_end_clean basic behavior
--FILE--
<?php
ob_start();
echo "Hello";
$c = ob_get_contents();
ob_end_clean();
echo $c . "\n";
?>
--EXPECT--
Hello
