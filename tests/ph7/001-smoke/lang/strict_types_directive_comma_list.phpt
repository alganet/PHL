--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
declare() accepts comma-separated directives with strict_types in any position
--FILE--
<?php
declare(strict_types=1, strict_types=1);
echo "ok\n";
?>
--EXPECT--
ok
