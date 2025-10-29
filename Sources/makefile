# Default target
all: clean feed manager

# Target to compile feed program
feed: feed.c
	gcc feed.c -o feed -pthread

# Target to compile broker program
manager: manager.c
	gcc manager.c -o manager -pthread
# Clean target to remove temporary files and executables
clean:
	rm -f feed manager PIPE_MANAGER PIPE_FEED* *.o *.pipe

.PHONY: all clean
