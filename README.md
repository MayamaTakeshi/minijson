# minijson
A minimal c json library

## Overview

This is a small lib that permits to parse json strings. There are lots of opensource json libs available, however, they will not compile in centos 3.9 (needed to for Excel CSP SK 8.3.1) without adjustments/workarounds so we decided to code our own.

The lib provides two interfaces:
  - full parser interface (parses the whole string at once)
  - pull parser interface (parses the string incrementally)

To undestand how to use it, read sample code at minijson_test.c
