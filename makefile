src = $(wildcard *.c) \
	$(wildcard lib/*.c)
obj = $(src:.c=.o)

LDFLAGS = -lm

tora: $(obj)
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) tora