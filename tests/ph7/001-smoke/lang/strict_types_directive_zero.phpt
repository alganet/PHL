--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
declare(strict_types=0) is an explicit opt-out and parses as the first statement
--FILE--
<?php
declare(strict_types=0);
echo "ok\n";
?>
--EXPECT--
ok
