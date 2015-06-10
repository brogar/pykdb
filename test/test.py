from pykdb import *

if __name__ == '__main__':
	try :
		conn = qserver.open_connection(host = 'localhost', port = 2100)
		print (conn)
		
		v = qserver.query(conn, 'a')
		print(v)
		
		print(qserver.close_connection(conn))
		
	except Exception as inst:
		print (inst)
		