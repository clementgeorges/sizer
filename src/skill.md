---
name: sizer
description: Resize a batch of images to one width (height follows, aspect ratio kept) and write the copies into a folder, with the sizer command. Use when asked to resize, shrink, or scale images or photos to a pixel width.
---

# Sizer

`sizer` writes resized copies of images into a folder. Originals are never
changed and existing files are never replaced.

```sh
sizer info photos/ --json                          # each image's width and height
sizer resize --width 1200 --out web/ photos/ --json
sizer resize -w 800 -o thumbs a.jpg b.png
```

- Arguments are image files or folders (a folder means the images directly in it).
- The height is computed from the width, keeping each image's shape.
- The copy keeps its name and format; when the name is taken in the output
  folder it becomes `name-1200w.jpg`. Formats Qt can't write (GIF) become PNG.
- EXIF rotation is applied, so portrait photos come out upright.
- Exit status is 0 when everything was written, 1 otherwise; errors go to stderr.
  `--json` prints `{"saved": [{source, destination, width, height}], "failed": [{source, error}]}`.
- `sizer open <images>` opens the window for the user to pick widths interactively.
