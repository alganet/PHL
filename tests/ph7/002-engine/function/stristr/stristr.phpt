--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: stristr case-insensitive search
--FILE--
<?php
echo "stristr=" . stristr('Hello', 'ell') . "\n";
?>
--EXPECT--
stristr=ello
