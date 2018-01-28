import random

class room():

        door = False
        status = False
        code = [1,2,3,4]
        y = random.shuffle(code)
        x = ''.join(str(x) for x in code)
        z = str(input('guess the 4-pin to unlock door: '))
Room1 = room()
count = 3
while Room1.door == False:
    print(Room1.z)
    if str(Room1.z) == Room1.x:
        Room1.door = True
        Room1.status = True
        print('Congrat. you unlock the pin, now door is opened')
        break
    else:
        print('Try again')
        count -=1
        print('You have {} atempts'.format(count))
        Room1.z = str(input('guess the 4-pin to unlock door: '))
        if count == 1:
            print('Game over')
            break
print('Room door is:',Room1.door, 'Room status still:', Room1.status, 'Code was:', Room1.x)
