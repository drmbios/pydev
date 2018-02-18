#created by drmbios aka nixster
import pip
import shutil
import os
print('...:::Welcome to qpipper:::...')
print('qpipper is a tool for downloading and installing python modules manually.')
print('')
print('Please select one of 2 options:')
print('')
x = str(input('Type search/install: '))
y = str(input('Type module name: '))

pip.main([x, y])
#os.chdir('/sdcard/qpython/lib/python3.2/site-packages/')
#fldr ='/sdcard/qpython/cache/pip-build/'
#os.mkdir(y, 0o755)
des = '/sdcard/qpython/lib/python3.2/site-packages/'
os.chdir('/sdcard/qpython/cache/pip-build/{}/'.format(y))
#os.listdir(fldr)
try:
        
    for file in y:
        shutil.move(y, des)
    print('{} module manually installed!'.format(y))
    print('All installation comlete, thx!')
except Exception:
    print('-' * 25)
    print('Module {} downloaded succesfully!'.format(y))
except OSError:
    print('Already downloaded and installed!!!')
except shutil.Error:
    print('Downloaded and installed succesfully')
    
else:
    try:
        pip.main([x, y])
        shutil.move(fldr, des)
        print('{} module download and manually installed!'.format(y))
        print('All installation comlete, thx!')
    except Exception:
        print('*' * 33)
        print('Module {} downloaded succesfully!'.format(y))
    except OSError:
        print('Already downloaded and installed!!!')
    except shutil.Error:
        print('Downloaded and installed succesfully')    
