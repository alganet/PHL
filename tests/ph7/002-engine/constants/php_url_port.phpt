--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: PHP_URL_PORT should be 2 or 3 (compat)
--FILE--
<?php
$php_url_port = (int)PHP_URL_PORT;
if ($php_url_port === 2 || $php_url_port === 3) {
    echo "PHP_URL_PORT=OK\n";
} else {
    echo "PHP_URL_PORT=FAIL\n";
}
?>
--EXPECT--
PHP_URL_PORT=OK
--CLEAN--
<?php
unset($php_url_port);
?>
