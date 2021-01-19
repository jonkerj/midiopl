curve = []
for vin in range(128):
	v = (1-(vin/127.0)) ** 2
	curve.append(int(v * 63))

print('const byte velocurve[128] = {')
bs = 16
for b in range(len(curve) // bs):
	batch = curve[b*bs:(b+1)*bs]
	prettybatch = ', '.join(map(lambda x: f'0x{x:02x}', batch))
	print(f'\t{prettybatch},')
print('};')
