#!/bin/bash

python3 codegen_test.py

coverage combine
coverage report -m
coverage html
