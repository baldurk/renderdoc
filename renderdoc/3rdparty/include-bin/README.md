# include-bin

C++ program that generates C arrays from binary data.
It's output is similar to `xxd -i`.

Extra features:

* uses sanitized input filename for variable names
* output lines are shorter than 79 characters


## Compilation
Just compile main.cpp with your favourite C++ compiler. This code is
compatible even with C++98 standard.

`g++ -O2 ./main.cpp -o include-bin`

## Usage

```bash
./include-bin [infile [outfile]]
```
If there is no outfile argument it will print on stdout. If there is also no
infile argument it will read from stdin. In latter case array name will be
"data".

Example:
```bash
sh-3.2$ hexdump te-st.txt
0000000 17 18 19 20 11 10 12 56 41 00 00 d0 d0 11 25 ff
0000010 ff 00 ff 00
0000014
sh-3.2$ ./include-bin te-st.txt out.c
sh-3.2$ cat out.c
unsigned char te_st_txt[] = {
  0x17, 0x18, 0x19, 0x20, 0x11, 0x10, 0x12, 0x56, 0x41, 0x00, 0x00, 0xd0,
  0xd0, 0x11, 0x25, 0xff, 0xff, 0x00, 0xff, 0x00
};
unsigned int te_st_txt_len = 20;
```

## License
This program is under [zlib license](https://en.wikipedia.org/wiki/Zlib_License).
