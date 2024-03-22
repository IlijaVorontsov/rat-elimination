# Removes 'd' lines and sorts literals by abs values, with the rat-pivot staying in first place.

import argparse

parser = argparse.ArgumentParser(description='Preprocess LRAT file')
parser.add_argument('file', type=str, help='Path to the LRAT file')
args = parser.parse_args()

file_path = args.file

with open(file_path, 'r') as file:
  lines = file.readlines()
  for line in lines:
    if 'd' in line:
      pass
    else:
      line = line.split()
      chain = [int(x) for x in line[line.index('0')+1:]]
      rat = False
      for c in chain:
        if c < 0:
          rat = True
          break
      if len(chain) == 0:
        rat = True

      if rat:
        print(line[0] + " " + line[1] + " " + " ".join(str(x) for x in sorted([int(x) for x in line[2:line.index('0')]], key=abs)) + " 0 " + " ".join(c for c in line[line.index('0')+1:]))
      else:
        print(line[0] + " " +  " ".join(str(x) for x in sorted([int(x) for x in line[1:line.index('0')]], key=abs)) + " 0 " + " ".join(c for c in line[line.index('0')+1:]))
