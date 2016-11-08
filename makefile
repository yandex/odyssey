all:
	@(cd lib;  make --no-print-directory)
	@(cd core;  make --no-print-directory)
clean:
	@(cd lib;  make --no-print-directory clean)
	@(cd core;  make --no-print-directory clean)
