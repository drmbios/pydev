letter = [tim for tim in open("/sdcard/qpython/prox/contacts.vcf").readlines()]
#ledds = dict((tim, 1) for tim in letter)
count = 0
#print(letter)
for red in letter:
    if "FN" in red:
        print(red)
    elif "CELL" in red:
        print(red)
    elif "WORK" in red:
        print(red)
    elif "TEL" in red:
        print(red)
        count += 1
print('Total records: {0}'.format(count))