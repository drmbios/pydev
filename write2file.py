def writer():
    fil = input('enter file name: ')
    with open(fil, 'w') as ff:
        ff.write(input('write some text: '))
        print(ff)
        print('File {} was saved, thanks'.format(fil))
    return ff
    
 
writer()
