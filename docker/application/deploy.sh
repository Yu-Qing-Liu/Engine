#!/bin/sh

cd ../../
rm -rf bin/ build/
sudo docker build -t engine .
