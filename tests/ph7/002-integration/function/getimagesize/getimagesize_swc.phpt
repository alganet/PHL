--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A compressed SWF is refused with php's own no-zlib sentence (PHL half of the twin pair)
--DESCRIPTION--
RECORDED SCOPE DIFFERENCE (the scope policy). "CWS" is the one container whose size lives
behind DEFLATE, and php reads it only when its build carries a STATIC zlib;
a php without one still names the type IMAGETYPE_SWC, still refuses the size,
and says exactly the sentence below. This engine links no zlib -- the same
scope cut that leaves the `compress.zlib` stream filter out -- so php's own
no-zlib branch is the honest answer here rather than a stub, and every other
face of the type (its number, its mime string, its extension) is php's.

The zend half of the pair pins the size a zlib-carrying php answers instead.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
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
bool(false)
8: getimagesizefromstring(): The image is a compressed SWF file, but you do not have a static version of the zlib extension enabled
IMAGETYPE_SWC=13 mime=application/x-shockwave-flash ext=.swf
