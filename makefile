BUILD_DIR = ./build
all: 
	mkdir $(BUILD_DIR);cd $(BUILD_DIR); cmake ../; make
install:
uninstall:
clean:
	rm -rf $(BUILD_DIR)
