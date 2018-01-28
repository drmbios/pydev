import json

def reader():
    fil = input('enter file name: ')
    ff = json.load(open(fil))
    for raw in ff:
        print(raw)
    return ff
    
 
reader()
