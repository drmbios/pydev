import sqlite3

connection = sqlite3.connect("/sdcard/qpython/jeo.db")
cursor = connection.cursor()
cursor.execute("""
	CREATE TABLE 'category'(
	'id' integer primary key,
	'name' text,
	'answer' text
		)
	""")

cursor.execute("insert into category(id, name, answer) values(1, Emin, Hello)")


connection.close()
