--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: internal ArgumentCountError is catchable as TypeError
--FILE--
<?php
try {
    abs();
    echo "no-error\n";
} catch (TypeError $e) {
    echo get_class($e) . "\n";
}
?>
--EXPECT--
ArgumentCountError
--CLEAN--
<?php

?>

