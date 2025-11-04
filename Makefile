# SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

build/ph7: src/ph7.c
	@mkdir -p build
	cc -o build/ph7 src/ph7.c examples/ph7_interp.c -W -Wunused -Wall -I. -Ofast

clean:
	rm -f build/ph7

test: build/ph7
	@sh -c '\
		test "$$(./build/ph7 scripts/hello_world.php)" = "Hello World!" &&\
			echo "Test passed." ||\
			echo "Test failed."'