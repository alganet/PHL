--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: PHP_QUERY_RFC1738 and PHP_QUERY_RFC3986
--FILE--
<?php
$one = (int)PHP_QUERY_RFC1738;
$two = (int)PHP_QUERY_RFC3986;
if ($one > 0 && $two > 0) {
    echo "PHP_QUERY_RFC=OK\n";
} else {
    echo "PHP_QUERY_RFC=FAIL\n";
}
?>
--EXPECT--
PHP_QUERY_RFC=OK
--CLEAN--
<?php
unset($one, $two);
