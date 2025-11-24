--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strip_tags removes HTML tags
--FILE--
<?php
echo strip_tags('<b>abc</b>') . "\n";
?>
--EXPECT--
abc
