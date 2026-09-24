--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the seven CRYPT_* capability constants exist with php's values
--FILE--
<?php
foreach (['CRYPT_SALT_LENGTH', 'CRYPT_STD_DES', 'CRYPT_EXT_DES', 'CRYPT_MD5',
          'CRYPT_BLOWFISH', 'CRYPT_SHA256', 'CRYPT_SHA512'] as $c) {
    echo $c, '=', constant($c), "\n";
}
?>
--EXPECT--
CRYPT_SALT_LENGTH=123
CRYPT_STD_DES=1
CRYPT_EXT_DES=1
CRYPT_MD5=1
CRYPT_BLOWFISH=1
CRYPT_SHA256=1
CRYPT_SHA512=1
--CLEAN--
<?php
