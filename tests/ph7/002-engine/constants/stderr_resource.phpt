--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: STDERR resource type
--FILE--
<?php
echo "STDERR=" . (is_resource(STDERR) ? "resource" : gettype(STDERR)) . "\n";
?>
--EXPECT--
STDERR=resource
