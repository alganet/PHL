--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: XML_SAX_IMPL constant string
--FILE--
<?php
if (function_exists('zend_version')) {
    if (XML_SAX_IMPL == 'libxml') {
        echo 'ok';
    } else {
        echo 'fail';
    }
} else {
    if (XML_SAX_IMPL == 'Symisc XML engine') {
        echo 'ok';
    } else {
        echo 'fail';
    }
}
?>
--EXPECT--
ok
