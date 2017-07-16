serve_vector_model = function(name, version, ip, port, fn) {
	ret = .Call("serve_numeric_vector_model", name, version, ip, port, fn, package="rclipper")
	return(ret)
}
