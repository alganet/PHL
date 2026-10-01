--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: a stream handle is closed when its LAST value goes away
--SKIPIF--
<?php
// The descriptor census reads /proc/self/fd, which only Linux has. The rest of
// the case is portable, but the leak it guards is only VISIBLE through a count.
if (!is_dir('/proc/self/fd')) { echo 'skip needs /proc/self/fd'; }
?>
--FILE--
<?php
function hlr_fds() { return count(scandir('/proc/self/fd')) - 2; }

$hlr_file = tempnam(sys_get_temp_dir(), 'hlr');
file_put_contents($hlr_file, "xyz");

/* Dropping the only value naming a handle releases the descriptor. */
$hlr_base = hlr_fds();
for ($hlr_i = 0; $hlr_i < 50; $hlr_i++) { $hlr_h = fopen($hlr_file, 'r'); $hlr_h = null; }
var_dump(hlr_fds() - $hlr_base);

/* A SECOND value keeps it open, and the handle both share is one handle. */
$hlr_a = fopen($hlr_file, 'r');
$hlr_b = $hlr_a;
$hlr_a = null;
var_dump(is_resource($hlr_b), fread($hlr_b, 3));
fclose($hlr_b);
var_dump(is_resource($hlr_b), gettype($hlr_b));

/* An array element and a by-value argument are values too. */
$hlr_arr = ['h' => fopen($hlr_file, 'r')];
var_dump(fread($hlr_arr['h'], 2));
unset($hlr_arr);
function hlr_read($hlr_p) { return fread($hlr_p, 1); }
$hlr_g = fopen($hlr_file, 'r');
var_dump(hlr_read($hlr_g), fread($hlr_g, 1));
fclose($hlr_g);

/* The standard handles outlive every value that names them. */
$hlr_o = STDOUT; $hlr_o = null; echo "stdout alive\n";

/* And a directory handle answers the same rule. The count is a BOUND rather
 * than a number: php's own census walks a directory of its own, so one run
 * differs from the next by a descriptor. Twenty dropped handles is the leak. */
$hlr_base = hlr_fds();
for ($hlr_i = 0; $hlr_i < 20; $hlr_i++) { $hlr_d = opendir(sys_get_temp_dir()); $hlr_d = null; }
var_dump(hlr_fds() - $hlr_base < 5);

@unlink($hlr_file);
?>
--EXPECT--
int(0)
bool(true)
string(3) "xyz"
bool(false)
string(17) "resource (closed)"
string(2) "xy"
string(1) "x"
string(1) "y"
stdout alive
bool(true)
--CLEAN--
<?php
unset($hlr_file, $hlr_base, $hlr_i, $hlr_h, $hlr_a, $hlr_b, $hlr_arr, $hlr_g, $hlr_o, $hlr_d);
