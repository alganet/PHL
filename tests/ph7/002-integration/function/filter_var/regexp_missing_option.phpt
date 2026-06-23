--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var FILTER_VALIDATE_REGEXP without a "regexp" option throws a ValueError
--FILE--
<?php
try {
    filter_var("x", FILTER_VALIDATE_REGEXP);
    echo "no-throw\n";
} catch (\ValueError $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
?>
--EXPECT--
ValueError: filter_var(): "regexp" option is missing
