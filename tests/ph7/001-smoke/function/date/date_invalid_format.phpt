--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
date with unknown format specifier

--FILE--
<?php
$result = date('q');
if ($result === 'q') {
    echo "PASS";
} else {
    echo "FAIL";
}
?>
--EXPECT--
PASS
--CLEAN--
<?php
unset($result);
