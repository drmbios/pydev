import re

ff = open('/sdcard/qpython/goo.html')
dd =''.join(str(x) for x in ff)

pat = r'href="(.*?)"' #for findall()

flags= re.IGNORECASE
finder = re.findall(pat, dd, flags)
print(finder)
#--------------

#pat = r'href=\"\w+\://w+.w+.+\"'

#finder = re.search(pat, dd)

#print(finder.group())