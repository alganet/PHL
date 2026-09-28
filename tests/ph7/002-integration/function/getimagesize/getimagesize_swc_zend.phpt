--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: a compressed SWF answers a size through zlib (zend half of the twin pair)
--DESCRIPTION--
The zend half of the pair: php's build here carries a static zlib, so the
DEFLATE stream behind "CWS" is decompressed and the frame RECT inside it read
like an uncompressed one. PHL links no zlib and answers php's own no-zlib
refusal; the PHL half of the pair pins that.
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
} elseif (!function_exists('gzcompress')) {
    echo "skip this php has no zlib either";
}
?>
--FILE--
<?php
/* A real compressed SWF: a 550x400 frame RECT and some incompressible noise
 * behind one DEFLATE stream, kept as a literal so both halves of the pair read
 * exactly the same bytes. */
$swc = base64_decode(
    'Q1dTBtgBAAB4AatgYI1nYOBfwEAmWH9E8pvz5enpMTv7xD0i87RudSx/5rKhvn0Ng7f5Lzbh' .
    'PTr7XS1mbN9a8HvvASWLk+fUWN51hj8+7BG8gfHAhLL5sWti/riLc71UPmDbPj3m+4OpKYsf' .
    'Olfffig8m0VYsVpu51PPuxfr5zcFb5okWu58epe+AE9s9stOH+e8vhoR0z/91soGLNonbv0y' .
    '035//ewm9ea5PWf8ZTvMlpUem6pzu/3iq/dHU7a8XCkr5fBn37VHPG0vFu3Oyiv5GPvslSCr' .
    'v+E2n8wXd7atX3oyfCrLbZ+X3mwVD6+Gzsu+Y2H8fXtlj0/ukuOhuzSf9WtbB9nPmdCq7KtQ' .
    'ev3+Qt0VJbf7dDO1EpTiC7Y/m8R5kjd0NaN83wWDmR/fLbx6+X+Qw1krrn9bfknP4fj+MOXm' .
    'mn+bzbRi2D/2N1x6/mbDAR1OvR2PNtSvZp064SV31gINFdc52bGGtnmvtny43x7z3mrTBj/9' .
    'un1nL2f6Sji3G75aoOgd7fIsvvb3R9s4v5zVPhMKq2Y9qHmdyLElLvH8tB1blXhYdMxFtDcK' .
    '7Zi6qXKa0NH076ytAGJuy1w=');
$msgs = [];
set_error_handler(function ($no, $msg) use (&$msgs) { $msgs[] = "$no: $msg"; return true; });
$r = getimagesizefromstring($swc);
restore_error_handler();
var_dump($r);
foreach ($msgs as $m) { echo $m, "\n"; }
echo "IMAGETYPE_SWC=", IMAGETYPE_SWC, " mime=", image_type_to_mime_type(IMAGETYPE_SWC),
     " ext=", image_type_to_extension(IMAGETYPE_SWC), "\n";
--EXPECT--
array(7) {
  [0]=>
  int(550)
  [1]=>
  int(400)
  [2]=>
  int(13)
  [3]=>
  string(24) "width="550" height="400""
  ["mime"]=>
  string(29) "application/x-shockwave-flash"
  ["width_unit"]=>
  string(2) "px"
  ["height_unit"]=>
  string(2) "px"
}
IMAGETYPE_SWC=13 mime=application/x-shockwave-flash ext=.swf
