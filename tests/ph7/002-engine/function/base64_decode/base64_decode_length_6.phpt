--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base64_decode with 6-character base64 string
--FILE--
<?php
echo base64_decode('YWJjZA') . "\n";
?>
--EXPECT--
abcd