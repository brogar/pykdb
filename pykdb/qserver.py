## minimal user visible functions

__all__ = [ 'open_connection', 'query', 'close_connection' ]

from pykdb.kdb import kx_py_close_connection, kx_py_execute, kx_py_open_connection

def open_connection(host, port, user = None):
	if user is None:
		_conn = kx_py_open_connection(host, port)
	else :
		_conn = kx_py_open_connection(host, port, user)
	return _conn
	
def close_connection(conn):
	_c = kx_py_close_connection(conn)
	return _c
	
def query(conn, query, *args):
	_qry = kx_py_execute(conn, query)
	return _qry
	