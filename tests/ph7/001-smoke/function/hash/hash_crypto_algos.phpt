--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hash() computes the cryptographic digests beyond md5/sha1/SHA-2 (md2, md4, the truncated SHA-512 pair, SHA-3 and RIPEMD), and every one of them can key a MAC
--FILE--
<?php
$algos = ['md2','md4','sha512/224','sha512/256','sha3-224','sha3-256','sha3-384','sha3-512',
          'ripemd128','ripemd160','ripemd256','ripemd320'];
// The published vectors: the empty string and "abc".
foreach ($algos as $a) {
    echo str_pad($a, 11), " ''=", hash($a, ""), "\n";
    echo str_pad($a, 11), " abc=", hash($a, "abc"), "\n";
}
// A truncated SHA-512 is NOT a prefix of the full one — FIPS 180-4 gives the
// two their own initial values precisely so it is not.
echo "512/256-is-prefix: ", var_export(hash('sha512/256', "abc") === substr(hash('sha512', "abc"), 0, 64), true), "\n";
// Digest widths, in raw bytes.
foreach ($algos as $a) {
    echo $a, "=", strlen(hash($a, "abc", true)), " ";
}
echo "\n";
// All of these are cryptographic, so all of them may key a MAC — including the
// SHA-3 rates, which absorb more per block (144) than any other algorithm.
$macs = hash_hmac_algos();
foreach ($algos as $a) {
    $listed = in_array($a, $macs, true);
    echo $a, " mac=", $listed ? "listed" : "MISSING", " ", hash_hmac($a, "the message", "key"), "\n";
}
// A key longer than the block is hashed down first; sha3-224's block is the
// widest one here, so this is the case a fixed 128-byte scratch would overrun.
echo "long-key sha3-224: ", hash_hmac('sha3-224', "x", str_repeat("K", 300)), "\n";
?>
--EXPECT--
md2         ''=8350e5a3e24c153df2275c9f80692773
md2         abc=da853b0d3f88d99b30283a69e6ded6bb
md4         ''=31d6cfe0d16ae931b73c59d7e0c089c0
md4         abc=a448017aaf21d8525fc10ae87aa6729d
sha512/224  ''=6ed0dd02806fa89e25de060c19d3ac86cabb87d6a0ddd05c333b84f4
sha512/224  abc=4634270f707b6a54daae7530460842e20e37ed265ceee9a43e8924aa
sha512/256  ''=c672b8d1ef56ed28ab87c3622c5114069bdd3ad7b8f9737498d0c01ecef0967a
sha512/256  abc=53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23
sha3-224    ''=6b4e03423667dbb73b6e15454f0eb1abd4597f9a1b078e3f5b5a6bc7
sha3-224    abc=e642824c3f8cf24ad09234ee7d3c766fc9a3a5168d0c94ad73b46fdf
sha3-256    ''=a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a
sha3-256    abc=3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532
sha3-384    ''=0c63a75b845e4f7d01107d852e4c2485c51a50aaaa94fc61995e71bbee983a2ac3713831264adb47fb6bd1e058d5f004
sha3-384    abc=ec01498288516fc926459f58e2c6ad8df9b473cb0fc08c2596da7cf0e49be4b298d88cea927ac7f539f1edf228376d25
sha3-512    ''=a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a615b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26
sha3-512    abc=b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e10e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0
ripemd128   ''=cdf26213a150dc3ecb610f18f6b38b46
ripemd128   abc=c14a12199c66e4ba84636b0f69144c77
ripemd160   ''=9c1185a5c5e9fc54612808977ee8f548b2258d31
ripemd160   abc=8eb208f7e05d987a9b044a8e98c6b087f15a0bfc
ripemd256   ''=02ba4c4e5f8ecd1877fc52d64d30e37a2d9774fb1e5d026380ae0168e3c5522d
ripemd256   abc=afbd6e228b9d8cbbcef5ca2d03e6dba10ac0bc7dcbe4680e1e42d2e975459b65
ripemd320   ''=22d65d5661536cdc75c1fdf5c6de7b41b9f27325ebc61e8557177d705a0ec880151c3a32a00899b8
ripemd320   abc=de4c01b3054f8930a79d09ae738e92301e5a17085beffdc1b8d116713e74f82fa942d64cdbc4682d
512/256-is-prefix: false
md2=16 md4=16 sha512/224=28 sha512/256=32 sha3-224=28 sha3-256=32 sha3-384=48 sha3-512=64 ripemd128=16 ripemd160=20 ripemd256=32 ripemd320=40 
md2 mac=listed 371066eb425c42d44fb1123fdc5fb941
md4 mac=listed 6692cd5898aab36bdddd742bf40ad9c0
sha512/224 mac=listed 99abfc7e4bede14f963dca88f7099f0711bc929d02ab057b8389008c
sha512/256 mac=listed 87843983f121bf84afa2f10e24af151c56f47b22e1023c69eec5726da5e94165
sha3-224 mac=listed 1894baabc59efafc3da9d327bf3bf7fba05021ececd89b92805d83d6
sha3-256 mac=listed e3195d5ae0a86296c2848ea3c90a46c19bd273e5fe5bf3d5e82e59b120d07637
sha3-384 mac=listed 321d0da8521de3d038a10a408390548a664d9588b6f2964cee17d6dc9d3a003848c2978891fc649668218ec46e717aac
sha3-512 mac=listed 91fbb1599afbcc18d898925aa43214b7f6c36a450abb8e5bd377a4af865ba3efb00f889749f54c6eb79b6df80a4fa3077c311707daba39fc0c29aef398543af1
ripemd128 mac=listed 44efce8d3f1b1f3ecdc93b9e68c083d5
ripemd160 mac=listed 8294863c9effe568c7f94fe6d372b7fc2dea52f5
ripemd256 mac=listed 8c6b39a564c11f5845ef85f44dd6a550a99f77a8872f27596e5d0b25599eb7f3
ripemd320 mac=listed aa013fe422ea9f86367a8ae4b088ace36e186cd7ab92fe4367220ba607f1a59aea337bb6329e1320
long-key sha3-224: f53dda73f7ec2c6a37d32437f3d6f33ce85118c852ac615c29f6864e
--CLEAN--
<?php
