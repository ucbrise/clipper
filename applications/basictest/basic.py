from jsonrpclib.SimpleJSONRPCServer import SimpleJSONRPCServer

def predict(*args):

	res = []
	for arg in args:
		try:
			lenval = len(arg)
		except TypeError:
			lenval = None
		res.append((lenval, arg))
	return res

def main():
	server = SimpleJSONRPCServer(('localhost', 1006))
	server.register_function(predict)
	print("Start server")
	server.serve_forever()
if __name__ == '__main__':  
    main()