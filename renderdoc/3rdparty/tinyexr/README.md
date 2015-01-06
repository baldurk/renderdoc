From https://github.com/syoyo/tinyexr

# Tiny OpenEXR image library.

![Example](https://github.com/syoyo/tinyexr/blob/master/asakusa.png?raw=true)

`tinyexr` is a small library to load and save OpenEXR(.exr) images.
`tinyexr` is written in portable C++(no library dependency except for STL), thus `tinyexr` is good to embed into your application.
To use `tinyexr`, simply copy `tinyexr.cc` and `tinyexr.h` into your project.

`tinyexr` currently supports:

* OpenEXR version 1.x.
* Normal image
  * Scanline format.
  * Uncompress("compress" = 0) and ZIP compression("compress" = 3).
  * Half pixel type.
* Deep image
  * Scanline format.
  * ZIPS compression("compress" = 2).
  * Half, float pixel type.
* Litte endian machine.
* C interface.
  * You can easily write language bindings(e.g. golang)
* EXR saving
  * with ZIP compression.

# Use case 

* mallie https://github.com/lighttransport/mallie
* Your project here!

## Usage

Reading ordinal EXR file.

```
  const char* input = "asakusa.exr";
  float* out; // width * height * RGBA
  int width;
  int height;
  const char* err;

  int ret = LoadEXR(&out, &width, &height, input, &err);
```

Reading deep image EXR file.
See `example/deepview` for actual usage.

```
  const char* input = "deepimage.exr";
  const char* err;
  DeepImage deepImage;

  int ret = LoadDeepEXR(&deepImage, input, &err);

  // acccess to each sample in the deep pixel.
  for (int y = 0; y < deepImage.height; y++) {
    int sampleNum = deepImage.offset_table[y][deepImage.width-1];
    for (int x = 0; x < deepImage.width-1; x++) {
      int s_start = deepImage.offset_table[y][x];
      int s_end   = deepImage.offset_table[y][x+1];
      if (s_start >= sampleNum) {
        continue;
      }
      s_end = (s_end < sampleNum) ? s_end : sampleNum;
      for (int s = s_start; s < s_end; s++) {
        float val = deepImage.image[depthChan][y][s];
        ...
      }
    }
  }

```

### Example

#### deepview 

`examples/deepview` is simple deep image viewer in OpenGL.

![DeepViewExample](https://github.com/syoyo/tinyexr/blob/master/examples/deepview/deepview_screencast.gif?raw=true)

## TODO

Contribution is welcome!

- [ ] Tile format.
- [ ] Support for various compression type.
- [X] Multi-channel.
- [ ] Multi-part(EXR2.0)
- [ ] Pixel order.
- [ ] Pixel format(UINT, FLOAT).
- [ ] Big endian machine.
- [ ] Optimization
  - [ ] ISPC?
  - [ ] multi-threading.

## Similar projects

* miniexr: https://github.com/aras-p/miniexr (Write OpenEXR)

## License

3-clause BSD

`tinyexr` uses miniz, which is developed by Rich Geldreich <richgel99@gmail.com>, and licensed under public domain.

## Author(s)

Syoyo Fujita(syoyo@lighttransport.com)

## Contributor(s)

* Matt Ebb (http://mattebb.com) : deep image example. Thanks!
* Matt Pharr (http://pharr.org/matt/) : Testing tinyexr with OpenEXR(IlmImf). Thanks! 
