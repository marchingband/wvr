import sys
import soundfile

data, fs = soundfile.read(sys.argv[1])
for x in data:
    print(x)
print(data.size)