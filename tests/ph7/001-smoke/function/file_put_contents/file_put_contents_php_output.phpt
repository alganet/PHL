--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fopen/fwrite with php://output stream
--FILE--
<?php
// Test fopen/fwrite with php://output stream to cover PHPStreamData_Write
$data = "Data written to php://output stream\n";

// Open php://output stream
$handle = fopen('php://output', 'w');
if ($handle === false) {
    echo "Failed to open php://output\n";
    exit(1);
}

// Write to the stream
$bytes_written = fwrite($handle, $data);

if ($bytes_written === false) {
    echo "Failed to write to php://output\n";
} else {
    echo "Successfully wrote " . $bytes_written . " bytes to php://output\n";
}

// Close the stream
$result = fclose($handle);
if ($result === false) {
    echo "Failed to close php://output\n";
} else {
    echo "Successfully closed php://output\n";
}

echo "php_output_test_completed\n";
?>
--EXPECT--
Data written to php://output stream
Successfully wrote 36 bytes to php://output
Successfully closed php://output
php_output_test_completed
--CLEAN--
<?php
unset($data, $handle, $bytes_written, $result);
