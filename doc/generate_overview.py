from sys import argv
import re

f = open(argv[-1], "r").read()

regex = r"(## Overview.*\n)## Building.*"

m = re.search(regex, f, re.MULTILINE | re.DOTALL)

subsection = m.groups()[0]

# Make headings one level higher
subsection = subsection.replace("## ", "# ")

print("Title: Overview\n")
print(subsection)
