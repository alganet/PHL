--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: STDOUT resource type
--FILE--
<?php
echo "STDOUT=" . (is_resource(STDOUT) ? "resource" : gettype(STDOUT)) . "\n";
?>
--EXPECT--
STDOUT=resource
