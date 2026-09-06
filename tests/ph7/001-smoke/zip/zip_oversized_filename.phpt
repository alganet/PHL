--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
zip_open/zip_read handles oversized filename length in central directory
--SKIPIF--
<?php
/* Skip on Zend PHP (this test targets the PH7/PHL zip implementation) */
if (function_exists('zend_version')) { echo "skip: not PH7\n"; }
if (!function_exists('zip_open')) { echo 'skip: zip not available'; }
?>
--FILE--
<?php
$fn = tempnam(sys_get_temp_dir(), 'ph7_zip');

// Create with oversized filename length field
// This is a malformed zip archive will trigger SXERR_BIG error in GetCentralDirectoryEntry() at line 289

// Minimal zip structure with corrupted central directory filename length
// End of central directory record (22 bytes)
$eocd = "\x50\x4b\x05\x06" .  // End of central dir signature
        "\x00\x00" .           // Number of this disk
        "\x00\x00" .           // Disk where central directory starts
        "\x01\x00" .           // Number of central directory records on this disk
        "\x01\x00" .           // Total number of central directory records
        "\x46\x00\x00\x00" .   // Size of central directory (70 bytes)
        "\x2e\x00\x00\x00" .   // Offset of central directory (46 bytes)
        "\x00\x00";            // Zip file comment length

// Central directory file header (46 bytes)
// We set an oversized filename length to trigger SXERR_BIG
$central = "\x50\x4b\x01\x02" .  // Central file header signature
           "\x14\x03" .           // Version made by
           "\x14\x00" .           // Version needed to extract
           "\x00\x00" .           // General purpose bit flag
           "\x00\x00" .           // Compression method
           "\x00\x00" .           // File last modification time
           "\x00\x00" .           // File last modification date
           "\x00\x00\x00\x00" .   // CRC-32
           "\x05\x00\x00\x00" .   // Compressed size (5 bytes)
           "\x05\x00\x00\x00" .   // Uncompressed size (5 bytes)
           "\xff\xff" .           // FILENAME LENGTH - MAX VALUE (65535) - triggers SXERR_BIG
           "\x00\x00" .           // Extra field length
           "\x00\x00" .           // File comment length
           "\x00\x00" .           // Disk number start
           "\x00\x00" .           // Internal file attributes
           "\x00\x00\x00\x00" .   // External file attributes
           "\x2e\x00\x00\x00" .   // Relative offset of local header
           "test";                // Filename (5 bytes)

// Local file header (30 bytes minimum)
$local = "\x50\x4b\x03\x04" .     // Local file header signature
          "\x14\x00" .            // Version needed to extract
          "\x00\x00" .            // General purpose bit flag
          "\x00\x00" .            // Compression method (stored)
          "\x00\x00" .            // Last mod file time
          "\x00\x00" .            // Last mod file date
          "\x00\x00\x00\x00" .    // CRC-32
          "\x05\x00\x00\x00" .    // Compressed size
          "\x05\x00\x00\x00" .    // Uncompressed size
          "\x04\x00" .            // Filename length (4 bytes)
          "\x00\x00" .            // Extra field length
          "test";                 // Filename

// Compressed data (5 bytes, stored)
$data = "hello";

// Combine all parts
$zip_data = $local . $data . $central . $eocd;

file_put_contents($fn, $zip_data);

// Try to open the malformed zip
$res = @zip_open($fn);
if ($res === false) {
    echo "status=ok\n";
    echo "zip_open correctly rejected malformed archive with oversized filename length\n";
} else {
    // If it somehow opened, try to read
    echo "status=warning\n";
    echo "zip_open did not reject malformed archive (unexpected)\n";
    @zip_close($res);
}

unlink($fn);
?>
--EXPECTF--
Error [8192]: Function zip_open() is deprecated since 8.0, use ZipArchive::open() instead in %s on line %d
status=ok
zip_open correctly rejected malformed archive with oversized filename length
--CLEAN--
<?php
unset($fn, $eocd, $central, $local, $data, $zip_data, $res);
