--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The #[ sequence inside string literals stays literal
--FILE--
<?php
echo "#[not an attribute]\n";
echo '#[single quoted]', "\n";
$s = "prefix #[inner] suffix";
echo $s, "\n";
?>
--EXPECT--
#[not an attribute]
#[single quoted]
prefix #[inner] suffix
--CLEAN--
<?php
?>
