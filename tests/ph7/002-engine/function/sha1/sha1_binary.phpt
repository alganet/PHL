--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sha1 with binary data
--FILE--
<?php
$data = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f";
echo sha1($data) . "\n";
?>
--EXPECT--
56178b86a57fac22899a9964185c2cc96e7da589