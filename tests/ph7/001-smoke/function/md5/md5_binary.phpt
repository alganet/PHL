--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
md5 with binary data
--FILE--
<?php
$data = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f";
echo md5($data) . "\n";
?>
--EXPECT--
1ac1ef01e96caf1be0d329331a4fc2a8
--CLEAN--
<?php
unset($data);
