--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base64_encode encodes a 2-byte string
--FILE--
<?php
echo base64_encode('ab') . "\n";
?>
--EXPECT--
YWI=