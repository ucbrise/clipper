MODEL_NAME = "rtest-model"
MODEL_VERSION = "1"

predict_func = function(inputs) {
	return(lapply(inputs, function(input) {
		return(as.character(length(input)))
	}))
}

Rclipper::build_model(MODEL_NAME, MODEL_VERSION, predict_func, as.numeric(7))
