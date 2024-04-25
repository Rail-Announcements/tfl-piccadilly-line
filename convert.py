import wave

data = open("copy file.pcc", "rb").read()
out = wave.open(open("out.wav", "wb"))
out.setnchannels(1)
out.setsampwidth(1)
out.setframerate(4000)
out.writeframes(bytes([(data[i] & 0b1000000) << 1 for i in range(1, len(data), 2)]))
