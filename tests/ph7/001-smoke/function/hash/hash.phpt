--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hash() computes md5/sha1/sha2-family digests (NIST 'abc' + empty string)
--FILE--
<?php
foreach (['md5','sha1','sha224','sha256','sha384','sha512'] as $a) {
    echo $a, " abc=", hash($a, "abc"), "\n";
    echo $a, " ''=", hash($a, ""), "\n";
}
?>
--EXPECT--
md5 abc=900150983cd24fb0d6963f7d28e17f72
md5 ''=d41d8cd98f00b204e9800998ecf8427e
sha1 abc=a9993e364706816aba3e25717850c26c9cd0d89d
sha1 ''=da39a3ee5e6b4b0d3255bfef95601890afd80709
sha224 abc=23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7
sha224 ''=d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f
sha256 abc=ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
sha256 ''=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
sha384 abc=cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7
sha384 ''=38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b
sha512 abc=ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f
sha512 ''=cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e
--CLEAN--
<?php
