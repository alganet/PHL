--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
declare(strict_types=1) parses cleanly as the first statement
--FILE--
<?php
declare(strict_types=1);
echo "ok\n";
?>
--EXPECT--
ok
