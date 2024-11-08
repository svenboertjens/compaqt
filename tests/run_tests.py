from subprocess import run

import os

run(["python", "tests/regular.py"])
run(["python", "tests/stream.py"])
run(["python", "tests/custom.py"])

