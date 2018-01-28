import string as st
story= open("/sdcard/qpython/pussboot.txt")

serber = ''.join(str(x) for x in story)


symbol = st.ascii_letters
story.read()
for raw in symbol:
    print(raw, serber.count(raw))
    
    
    
    
    
    
 