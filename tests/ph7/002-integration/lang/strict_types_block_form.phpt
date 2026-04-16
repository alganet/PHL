--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
declare(strict_types) cannot be used with the block form
--FILE--
<?php
declare(strict_types=1) {
    echo "inside\n";
}
?>
--EXPECTF--
%AFatal error:%Astrict_types declaration must not use block mode%A
