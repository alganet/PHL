--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
stream_get_contents rejects a length below -1 with a ValueError; -1 reads to EOF
--FILE--
<?php
$fp = fopen('php://memory', 'r+');
fwrite($fp, 'Hello World');
rewind($fp);
try {
    stream_get_contents($fp, -2);
    echo "NO_ERROR\n";
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
rewind($fp);
echo stream_get_contents($fp, -1), "\n";
fclose($fp);
?>
--EXPECT--
stream_get_contents(): Argument #2 ($length) must be greater than or equal to -1
Hello World
--CLEAN--
<?php
