--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
popen with write mode (unix)
--SKIPIF--
<?php if(PHP_OS == 'WINNT') { echo 'skip'; } ?>
--FILE--
<?php
// Test popen with write mode to cover PipeStream_Write
$command = 'cat'; // Simple command that reads from stdin and writes to stdout

// Open pipe for writing
$handle = popen($command, 'w');
if ($handle === false) {
    echo "Failed to open pipe\n";
    exit(1);
}

// Write data to the pipe
$data = "Hello from popen write test\n";
$bytes_written = fwrite($handle, $data);

if ($bytes_written === false) {
    echo "Failed to write to pipe\n";
} else {
    echo "Successfully wrote " . $bytes_written . " bytes to pipe\n";
}

// Close the pipe
$result = pclose($handle);
echo "pclose returned: " . $result . "\n";

echo "popen_write_test_completed\n";
?>
--EXPECT--
Successfully wrote 28 bytes to pipe
Hello from popen write test
pclose returned: 0
popen_write_test_completed
