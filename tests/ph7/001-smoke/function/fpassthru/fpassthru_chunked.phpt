--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fpassthru outputs each chunk it reads, not the running total
--FILE--
<?php
// More than one read buffer, so the second chunk is the one that used to be
// written with the accumulated length: an out-of-bounds read whose overrun
// reached the output.
$fp_chunk_name = tempnam(sys_get_temp_dir(), 'ph7_passbig_');
$fp_chunk_data = str_repeat('AB', 6000);
file_put_contents($fp_chunk_name, $fp_chunk_data);

$fp_chunk_fp = fopen($fp_chunk_name, 'rb');
fread($fp_chunk_fp, 3);          /* pass through from a non-zero position */
ob_start();
$fp_chunk_n = fpassthru($fp_chunk_fp);
$fp_chunk_out = ob_get_clean();
fclose($fp_chunk_fp);

var_dump($fp_chunk_n);
var_dump(strlen($fp_chunk_out));
var_dump($fp_chunk_out === substr($fp_chunk_data, 3));
?>
--EXPECT--
int(11997)
int(11997)
bool(true)
--CLEAN--
<?php
@unlink($fp_chunk_name);
unset($fp_chunk_name, $fp_chunk_data, $fp_chunk_fp, $fp_chunk_n, $fp_chunk_out);
