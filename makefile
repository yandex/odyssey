all:
	@(cd lib; make --no-print-directory)
	@(cd machinarium; make --no-print-directory)
clean:
	@(cd lib; make --no-print-directory clean)
	@(cd machinarium; make --no-print-directory clean)
