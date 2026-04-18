#!/usr/bin/env python3
import os

if __name__ == "__main__":
    os.system("python -m http.server 8000 --directory build_host_coverage/coverage")
