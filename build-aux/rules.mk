# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

$(BUILD_DIR)/$(MODE)/src/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(MODE_CFLAGS) -c $< -o $@
