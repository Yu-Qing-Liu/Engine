#!/bin/sh

docker run --rm -it \
  --name engine \
  engine \
  xvfb-run -s "-screen 0 1280x800x24" ./Engine
