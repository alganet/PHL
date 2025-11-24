--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: STDIN resource type
--FILE--
<?php
echo "STDIN=" . (is_resource(STDIN) ? "resource" : gettype(STDIN)) . "\n";
?>
--EXPECT--
STDIN=resource
