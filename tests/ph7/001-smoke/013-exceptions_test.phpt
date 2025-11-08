--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Exceptions
--FILE--
<?php
try {
    throw new Exception('test');
} catch (Exception $e) {
    echo $e->getMessage();
}
?>
--EXPECT--
test
--CLEAN--
<?php
unset($e);
?>